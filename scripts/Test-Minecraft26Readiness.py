"""Audit the 1.21.11 JNI contract against official, unobfuscated 26.x client jars.

Read-only analysis, NOT a supported-target generator. --download fetches only public
Mojang/Fabric development fixtures into an explicit local cache. No game is run,
production resource accessed, DLL produced, or runtime fallback enabled.
"""
import argparse
from collections import Counter
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import struct
import tempfile
import urllib.request
import zipfile

from minecraft_classfile import ClassArchive, read_intermediary, read_proguard, remap_descriptor

ROOT = Path(__file__).resolve().parent.parent
TARGETS = ("26.1", "26.1.1", "26.1.2", "26.2", "26.3")
MANIFEST = "https://piston-meta.mojang.com/mc/game/version_manifest_v2.json"


def fetch_json(url):
    with urllib.request.urlopen(url, timeout=45) as response:
        return json.load(response)


def download(url, path, expected_sha1=None, expected_size=None):
    if not path.exists():
        temporary = None
        try:
            with urllib.request.urlopen(url, timeout=45) as response, tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as out:
                temporary = Path(out.name)
                shutil.copyfileobj(response, out)
            validate_file(temporary, expected_sha1, expected_size)
            temporary.replace(path)
        finally:
            if temporary is not None:
                temporary.unlink(missing_ok=True)
    validate_file(path, expected_sha1, expected_size)


def validate_file(path, sha1, size):
    if size is not None and path.stat().st_size != size:
        raise ValueError(f"Fixture size mismatch: {path.name}")
    if sha1 is not None:
        with path.open("rb") as source:
            actual = hashlib.file_digest(source, "sha1").hexdigest()
        if actual != sha1:
            raise ValueError(f"Fixture SHA-1 mismatch: {path.name}")


def fetch_fixtures(cache, targets):
    cache.mkdir(parents=True, exist_ok=True)
    manifest = fetch_json(MANIFEST)
    versions = {v["id"]: v for v in manifest["versions"]}
    for version in ("1.21.11", *targets):
        release = versions[version]
        if release["type"] != "release":
            raise ValueError("This audit only accepts stable releases")
        metadata = fetch_json(release["url"])
        (cache / f"version-{version}.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
        items = [("client", f"client-{version}.jar")]
        if version == "1.21.11": items.append(("client_mappings", "client-1.21.11.txt"))
        for key, filename in items:
            info = metadata["downloads"][key]
            download(info["url"], cache / filename, info["sha1"], info["size"])
    download("https://maven.fabricmc.net/net/fabricmc/intermediary/1.21.11/intermediary-1.21.11-v2.jar",
             cache / "intermediary-1.21.11.jar")


def compile_bindings(compiler, native_261=False):
    msvc = Path(compiler).name.lower() in ("cl", "cl.exe", "clang-cl", "clang-cl.exe")
    with tempfile.TemporaryDirectory(prefix="onestep-26-bindings-") as directory:
        output = Path(directory) / ("bindings.exe" if os.name == "nt" else "bindings")
        command = [compiler]
        target = "26100" if native_261 else "12111"
        command += (["/nologo", "/std:c++20", "/EHsc", f"/DONESTEP_MINECRAFT_VERSION={target}", f"/Fe:{output}"] if msvc
                    else ["-std=c++20", "-pthread", f"-DONESTEP_MINECRAFT_VERSION={target}", "-o", str(output)])
        for include in (ROOT / "include/jni", ROOT / "include/jni" / ("win32" if os.name == "nt" else "linux")):
            command += [f"/I{include}"] if msvc else ["-I", str(include)]
        subprocess.run(command + [str(ROOT / "tests/minecraft_bindings_dump.cpp")], cwd=directory, check=True)
        result = subprocess.run([str(output), "--contracts" if native_261 else "--owners"], capture_output=True, text=True, check=True)
    return [line.split("\t") for line in result.stdout.splitlines()]


def project_binding(binding, intermediary_classes, intermediary_members, mojang_classes, mojang_members, base):
    owner, name, descriptor = binding
    kind = "method" if descriptor.startswith("(") else "field"
    inverse = {intermediary: obfuscated for obfuscated, intermediary in intermediary_classes.items()}
    obfuscated_owner = inverse.get(owner, owner)
    obfuscated_descriptor = remap_descriptor(descriptor, inverse)
    lineage = set(base.lineage(obfuscated_owner))
    candidates = intermediary_members.get((kind, name, descriptor), [])
    if not candidates:
        candidates = [(obfuscated_owner, name, obfuscated_descriptor)]
    values = set()
    for declaring, obfuscated_name, mapped_descriptor in candidates:
        if declaring not in lineage: continue
        found = base.find(obfuscated_owner, kind, obfuscated_name, mapped_descriptor)
        if not found: continue
        declaration, flags = found
        named = mojang_members.get((declaration, kind, obfuscated_name, mapped_descriptor))
        if named:
            _, named_name, named_descriptor = named
            values.add((mojang_classes.get(obfuscated_owner, obfuscated_owner), named_name, named_descriptor, flags & 8))
        elif not name.startswith(("method_", "field_", "comp_")):
            values.add((mojang_classes.get(obfuscated_owner, obfuscated_owner), name,
                        remap_descriptor(mapped_descriptor, mojang_classes), flags & 8))
    if len(values) != 1:
        raise ValueError(f"Unresolved/ambiguous BASELINE binding: {owner}.{name}{descriptor}")
    official_owner, official_name, official_descriptor, static = values.pop()
    return {"source_owner": owner, "source_name": name, "source_descriptor": descriptor,
            "owner": official_owner, "kind": kind, "name": official_name,
            "descriptor": official_descriptor, "static": bool(static)}


def inspect_binding(binding, target):
    owner, kind, name, descriptor = (binding[k] for k in ("owner", "kind", "name", "descriptor"))
    if not target.get(owner):
        # Hints only: a matching basename is not evidence of semantic compatibility.
        suffix = "/" + owner.rsplit("/", 1)[-1] + ".class"
        candidates = sorted(n[:-6] for n in target.jar.namelist() if n.endswith(suffix))
        return {**binding, "status": "missing_class", "candidate_classes": candidates}
    found = target.find(owner, kind, name, descriptor)
    if found:
        declaration, flags = found
        status = "signature_present" if bool(flags & 8) == binding["static"] else "static_changed"
        return {**binding, "status": status, "declared_in": declaration}
    alternatives = sorted({d for current in target.lineage(owner) if (info := target.get(current))
                           for k, n, d in info.members if k == kind and n == name})
    return {**binding, "status": "descriptor_changed" if alternatives else "missing_member",
            "candidate_descriptors": alternatives}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache-dir", required=True, type=Path)
    parser.add_argument("--download", action="store_true", help="Fetch public development fixtures; otherwise entirely offline")
    parser.add_argument("--target", choices=TARGETS, action="append")
    parser.add_argument("--native-261", action="store_true", help="Validate the critical native 26.1 contracts, failing on any missing signature/static mismatch; does not build a DLL")
    parser.add_argument("--output", required=True, type=Path, help="Write machine-readable audit JSON, never generated runtime bindings")
    parser.add_argument("--compiler", default=os.environ.get("CXX", "cl" if os.name == "nt" else "g++"))
    args = parser.parse_args()
    compiler = shutil.which(args.compiler)
    if not compiler: parser.error("C++20 compiler required")
    targets = tuple(dict.fromkeys(args.target or (("26.1",) if args.native_261 else TARGETS)))
    if args.native_261 and targets != ("26.1",): parser.error("--native-261 only supports exact target 26.1")
    try:
        if args.download: fetch_fixtures(args.cache_dir, targets)
        # Validate cached official artifacts even in the offline path.
        metadata = {}
        for version in targets if args.native_261 else ("1.21.11", *targets):
            info = json.loads((args.cache_dir / f"version-{version}.json").read_text(encoding="utf-8"))
            if info["id"] != version or info["type"] != "release":
                raise ValueError("Wrong release metadata in cache")
            metadata[version] = info
            for key, filename in [("client", f"client-{version}.jar")] + ([("client_mappings", "client-1.21.11.txt")] if version == "1.21.11" else []):
                entry = info["downloads"][key]
                validate_file(args.cache_dir / filename, entry["sha1"], entry["size"])
        if args.native_261:
            projected = [{"owner": owner, "kind": "method" if descriptor.startswith("(") else "field",
                          "name": name, "descriptor": descriptor, "static": bool(int(static))}
                         for owner, name, descriptor, static in compile_bindings(compiler, native_261=True)]
        else:
            classes, members = read_intermediary(args.cache_dir / "intermediary-1.21.11.jar")
            mojang_classes, mojang_members = read_proguard((args.cache_dir / "client-1.21.11.txt").read_text(encoding="utf-8"))
            with ClassArchive(args.cache_dir / "client-1.21.11.jar") as base:
                projected = [project_binding(b, classes, members, mojang_classes, mojang_members, base) for b in compile_bindings(compiler)]
        report = {"baseline": "native-26.1" if args.native_261 else "1.21.11", "scope": "compiled critical JNI bindings only; excludes Java bridges, renderer hooks, runtime semantics and gameplay",
                  "supported": False, "targets": {}}
        for version in targets:
            with ClassArchive(args.cache_dir / f"client-{version}.jar") as target:
                bindings = [inspect_binding(b, target) for b in projected]
                client = target.get("net/minecraft/client/Minecraft")
                if not client: raise ValueError(f"Missing official Minecraft class in {version}")
                if target.get("net/minecraft/class_310"):
                    raise ValueError(f"Unexpected intermediary namespace in {version}")
                counts = dict(Counter(b["status"] for b in bindings))
                report["targets"][version] = {"java": metadata[version]["javaVersion"]["majorVersion"],
                    "classfile_major": client.major, "namespace": "mojang", "counts": counts, "bindings": bindings}
                print(f"{version}: Java {report['targets'][version]['java']}, classfile {client.major}, {counts}")
        args.output.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        print("Audit complete, NOT runtime compatibility or a build target. Report:", args.output)
        if args.native_261:
            return 0 if all(b["status"] == "signature_present" for t in report["targets"].values() for b in t["bindings"]) else 1
        return 0
    except (OSError, ValueError, KeyError, struct.error, zipfile.BadZipFile, subprocess.CalledProcessError) as error:
        parser.exit(2, f"Audit failed: {error}\n")


if __name__ == "__main__":
    raise SystemExit(main())
