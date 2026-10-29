"""Audit every preprocessed 26.1 SDK declaration against local classfiles.

GCC/Clang preprocessing selects real C++ branches; a compiled probe supplies JNI
owners, descriptors and static flags. No game code, DLL or network is used.
"""
import argparse
from collections import Counter
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import zipfile

from minecraft_classfile import ClassArchive, read_class

ROOT = Path(__file__).resolve().parent.parent
TARGETS = {"26.1": 26100, "26.1.1": 26101, "26.1.2": 26102}
HEADERS = {p.relative_to(ROOT).as_posix(): "maps" for p in (ROOT / "src/sdk/minecraft/common").glob("*.hpp")}
HEADERS.update({"src/modules/visual/chams_model.hpp": "chams_model",
                "src/modules/visual/sky_bindings.hpp": "sky_bindings",
                "src/modules/vape/game_bindings.hpp": "vape_bindings"})


def closing(text, start, opening, end):
    depth, quote, escaped = 0, None, False
    for index in range(start, len(text)):
        char = text[index]
        if quote:
            if escaped: escaped = False
            elif char == "\\": escaped = True
            elif char == quote: quote = None
        elif char in "\"'": quote = char
        elif char == opening: depth += 1
        elif char == end:
            depth -= 1
            if depth == 0: return index
    raise ValueError("Unbalanced SDK declaration")


def declarations(text, namespace):
    classes, members = [], []
    tokens = re.compile(r'\b(BEGIN_KLASS_DEF(?:_EX)?|BEGIN_KLASS_MEMBERS(?:_EX)?|KLASS_DECLARATION)\(\s*([\w$]+)'
                        r'|\b(END_KLASS_DEF|END_KLASS_MEMBERS)\(\)')
    owner, begin = None, None
    for token in tokens.finditer(text):
        kind, name, end = token.groups()
        if end:
            if owner is None: raise ValueError("SDK end without owner")
            body = text[begin:token.start()]
            nested = [(m[1], m.end() - 1, closing(body, m.end() - 1, "{", "}"))
                      for m in re.finditer(r'\bstruct\s+(\w+)\s*\{', body)]
            for member in re.finditer(r'\bjni::(field|method|constructor|multi_field|multi_method)\s*<', body):
                finish = closing(body, member.end() - 1, "<", ">")
                name = re.match(r'\s*(\w+)\s*(?:\{|;)', body[finish + 1:])
                if not name:
                    if re.match(r'\s*\{', body[finish + 1:]): continue  # temporary; same contract has a named declaration
                    raise ValueError("Unsupported SDK binding declaration in " + owner)
                if member[1].startswith("multi_"): raise ValueError("Audit must explicitly handle multi-name bindings")
                scopes = [n for n, lo, hi in nested if lo < member.start() < hi]
                members.append((owner, "::".join([*scopes, name[1]])))
            owner, begin = None, None
        elif kind == "KLASS_DECLARATION":
            classes.append(namespace + "::" + name)
        else:
            if owner is not None: raise ValueError("Nested SDK class declaration")
            owner, begin = namespace + "::" + name, token.end()
            classes.append(owner)
    if owner is not None: raise ValueError("Unterminated SDK owner")
    return sorted(set(classes)), sorted(set(members))


def selected_sources(preprocessed):
    sources = {path: [] for path in HEADERS}
    current = None
    for line in preprocessed.splitlines():
        marker = re.match(r'#\s+\d+\s+"([^"]+)"', line)
        if marker:
            path = Path(marker[1])
            if not path.is_absolute(): path = ROOT / path
            try: current = path.resolve().relative_to(ROOT).as_posix()
            except ValueError: current = None
        elif current in sources:
            sources[current].append(line)
    if any(not lines for lines in sources.values()): raise ValueError("Preprocessor omitted an SDK header")
    return {path: "\n".join(lines) for path, lines in sources.items()}


def compile_contracts(compiler, target_id=26100):
    # A new private binding header must not silently fall outside this audit.
    located = {p.relative_to(ROOT).as_posix() for p in (ROOT / "src").rglob("*")
               if p.suffix in (".hpp", ".cpp") and p.name != "meta_jni.hpp"
               and re.search(r'\b(?:BEGIN_KLASS_DEF|BEGIN_KLASS_MEMBERS|KLASS_DECLARATION)|\bjni::(?:field|method|constructor|multi_field|multi_method)\s*<',
                             p.read_text(encoding="utf-8"))}
    if located != set(HEADERS): raise ValueError("SDK source inventory changed: " + str(sorted(located ^ set(HEADERS))))
    with tempfile.TemporaryDirectory(prefix="onestep-full-sdk-") as directory:
        work = Path(directory)
        probe = work / "probe.cpp"
        includes = '\n'.join('#include "' + (ROOT / path).as_posix() + '"' for path in (
            "src/mappings.hpp", "src/modules/visual/chams_model.hpp", "src/modules/visual/sky_bindings.hpp", "src/modules/vape/game_bindings.hpp"))
        probe.write_text(includes + "\n", encoding="utf-8")
        common = [compiler, "-std=c++20", "-pthread", f"-DONESTEP_MINECRAFT_VERSION={target_id}",
                  "-I", str(ROOT / "include/jni"), "-I", str(ROOT / "include/jni/linux")]
        pre = subprocess.run(common + ["-E", "-fdirectives-only", str(probe)], cwd=ROOT, capture_output=True, text=True, check=True)
        classes, members = [], []
        for path, source in selected_sources(pre.stdout).items():
            c, m = declarations(source, HEADERS[path]); classes.extend(c); members.extend(m)
        classes, members = sorted(set(classes)), sorted(set(members))
        program = includes + r'''
#include <iostream>
template<class Owner> void type(const char* cpp) {
    std::cout << "class\t" << cpp << '\t' << static_cast<const char*>(Owner::get_name()) << '\n';
}
template<class Owner, class Member> void binding(const char* cpp) {
    constexpr auto descriptor = Member::get_signature();
    constexpr bool method = descriptor.value[0] == '(';
    std::cout << (method ? "method\t" : "field\t") << cpp << '\t'
        << static_cast<const char*>(Owner::get_name()) << '\t' << static_cast<const char*>(Member::get_name())
        << '\t' << static_cast<const char*>(descriptor) << '\t';
    if constexpr (method) std::cout << Member::is_method_static();
    else std::cout << Member::is_field_static();
    std::cout << '\n';
}
int main() {
'''
        program += '\n'.join(f'type<{owner}>("{owner}");' for owner in classes)
        program += '\n' + '\n'.join(f'binding<{owner}, decltype({owner}_members::{member})>("{owner}.{member}");' for owner, member in members)
        probe.write_text(program + "\n}\n", encoding="utf-8")
        output = work / "contracts"
        subprocess.run(common + [str(probe), "-o", str(output)], cwd=work, check=True)
        result = subprocess.run([str(output)], capture_output=True, text=True, check=True)
    return [line.split("\t") for line in result.stdout.splitlines()]


RAW_OWNERS = {
    "src/utils/version_manager.cpp": {"loader": "net/fabricmc/loader/api/FabricLoader", "optional_type": "java/util/Optional",
        "container_type": "net/fabricmc/loader/api/ModContainer", "metadata_type": "net/fabricmc/loader/api/metadata/ModMetadata",
        "value_type": "net/fabricmc/loader/api/Version"},
    "src/platform/adapter/game_adapter.cpp": {"gr_cls": "net/minecraft/client/renderer/GameRenderer", "cam_cls": "net/minecraft/client/Camera", "num_cls": "java/lang/Number"},
    "src/modules/visual/player_esp.cpp": {"num_cls": "java/lang/Number"},
    "src/modules/visual/full_bright.cpp": {"double_class": "java/lang/Double"},
    "src/modules/visual/sky_panorama.cpp": {"loader_class": "java/lang/ClassLoader", "fog_bridge": "net/onestep/visual/SkyFogBridge",
        "transformer": "net/onestep/visual/SkyTransformer", "class_type": "java/lang/Class", "loader_type": "java/net/URLClassLoader", "bridge": "net/onestep/visual/SkyBridge"},
}


def arguments(text):
    """Split this repository's JNI arguments without splitting quoted descriptors/macros."""
    result, begin, depth, quote, escaped = [], 0, 0, None, False
    for i, char in enumerate(text):
        if quote:
            if escaped: escaped = False
            elif char == "\\": escaped = True
            elif char == quote: quote = None
        elif char in "\"'": quote = char
        elif char == "(": depth += 1
        elif char == ")": depth -= 1
        elif char == "," and depth == 0: result.append(text[begin:i].strip()); begin = i + 1
    return [*result, text[begin:].strip()]


def raw_contracts(compiler, records, target_id=26100):
    generated = (ROOT / "src/sdk/minecraft/fabric_26_1/official_names.hpp").read_text(encoding="utf-8")
    symbols = dict(re.findall(r'symbol<"([^"]+)">.*?string_litteral\("([^"]+)"\)', generated))
    camera = next(r[3] for r in records if r[1] == "maps::Camera.getPos")
    def literal(value):
        if value == "maps::bindings_profile::camera_position": return camera
        mapped = re.fullmatch(r'ONESTEP_MC_NAME\("([^"]+)"\)', value)
        if mapped: return symbols[mapped[1]]
        plain = re.fullmatch(r'"([^"]+)"', value)
        if plain: return plain[1]
        raise ValueError("Unreviewed raw JNI expression: " + value)
    raw = []
    found_files = set()
    for path in (ROOT / "src").rglob("*.cpp"):
        source = path.read_text(encoding="utf-8")
        if not re.search(r'->(?:Get(?:Static)?(?:Method|Field)ID|FindClass|RegisterNatives)\(', source): continue
        relative = path.relative_to(ROOT).as_posix()
        if relative not in RAW_OWNERS: raise ValueError("Unreviewed raw JNI source: " + relative)
        found_files.add(relative)
        # Only select conditional branches here; do not emulate Windows headers.
        source = re.sub(r'^\s*#\s*include[^\n]*', '', source, flags=re.M)
        pre = subprocess.run([compiler, "-E", "-P", "-x", "c++", f"-DONESTEP_MINECRAFT_VERSION={target_id}",
                              "-include", str(ROOT / "src/platform/adapter/target_version.hpp"), "-"],
                             input=source, capture_output=True, text=True, check=True).stdout
        for call in re.finditer(r'->(Get(?:Static)?(?:Method|Field)ID|FindClass)\(', pre):
            end = closing(pre, call.end() - 1, "(", ")")
            args = arguments(pre[call.end():end])
            label = f"{relative}:{len(raw)}"
            if call[1] == "FindClass":
                raw.append(["class", label, literal(args[0])]); continue
            if len(args) != 3: raise ValueError("Unexpected JNI argument count")
            owner = RAW_OWNERS[relative].get(args[0])
            if owner is None: raise ValueError(f"Unreviewed JNI owner: {relative}:{args[0]}")
            name, descriptor = literal(args[1]), literal(args[2])
            raw.append(["method" if "Method" in call[1] else "field", label + ":" + name, owner, name, descriptor,
                        "1" if "Static" in call[1] else "0"])
        # Native class-loader lookups are also checked, including the font/fog hooks.
        for expression in re.findall(r'ONESTEP_MC_NAME\("(net[/.]minecraft[/.]class_\d+)"\)', pre):
            raw.append(["class", relative + ":" + expression, symbols[expression].replace(".", "/")])
        registrations = re.findall(r'const_cast<char\*>\("([^"]+)"\)\s*,\s*const_cast<char\*>\("([^"]+)"\)\s*,\s*reinterpret_cast<void\*>\(&runtime::\w+\)', pre)
        if "->RegisterNatives(" in pre and (relative != "src/modules/visual/sky_panorama.cpp" or len(registrations) != 2):
            raise ValueError("Unreviewed native registration table: " + relative)
        for name, descriptor in registrations:
            raw.append(["method", relative + ":RegisterNatives:" + name, RAW_OWNERS[relative]["bridge"], name, descriptor, "1"])
    if found_files != set(RAW_OWNERS): raise ValueError("Raw JNI source inventory changed")
    return raw


class Universe:
    def __init__(self, paths, java_home):
        self.archives = [ClassArchive(path) for path in paths]
        jmod = java_home / "jmods/java.base.jmod"
        self.jmod = zipfile.ZipFile(jmod) if jmod.is_file() else None
        self.extracted = None
        if self.jmod is None:
            self.extracted = tempfile.TemporaryDirectory(prefix="onestep-java-base-")
            subprocess.run([str(java_home / "bin/jimage"), "extract", "--include", "glob:/java.base/**",
                            "--dir", self.extracted.name, str(java_home / "lib/modules")], check=True)
        self.cache = {}

    def close(self):
        for archive in self.archives: archive.close()
        if self.jmod: self.jmod.close()
        if self.extracted: self.extracted.cleanup()

    def get(self, name):
        if name not in self.cache:
            value = next((info for archive in self.archives if (info := archive.get(name))), None)
            if value is None:
                for root in (ROOT / "assets/skybridge",):
                    resource = root / (name + ".class")
                    if resource.is_file(): value = read_class(resource.read_bytes()); break
            if value is None:
                if self.jmod:
                    try: value = read_class(self.jmod.read("classes/" + name + ".class"))
                    except KeyError: pass
                else:
                    path = Path(self.extracted.name) / "java.base" / (name + ".class")
                    if path.is_file(): value = read_class(path.read_bytes())
            self.cache[name] = value
        return self.cache[name]

    lineage = ClassArchive.lineage
    find = ClassArchive.find


def inspect(records, universe):
    results = []
    for record in records:
        kind, cpp, owner, *rest = record
        result = {"kind": kind, "cpp": cpp, "owner": owner}
        if not universe.get(owner): result["status"] = "missing_class"
        elif kind == "class": result["status"] = "present"
        else:
            name, descriptor, static = rest
            result.update(name=name, descriptor=descriptor, static=bool(int(static)))
            found = universe.find(owner, kind, name, descriptor)
            if found:
                declaration, flags = found
                result["declared_in"] = declaration
                result["status"] = "present" if bool(flags & 8) == bool(int(static)) else "wrong_static"
                if ":RegisterNatives:" in cpp and not flags & 256: result["status"] = "not_native"
                missing_types = sorted({name for name in re.findall(r'L([^;]+);', descriptor) if not universe.get(name)})
                if missing_types:
                    result["status"], result["missing_types"] = "missing_descriptor_type", missing_types
            else:
                result["status"] = "missing_member"
                result["alternatives"] = sorted({n + d for current in universe.lineage(owner) if (info := universe.get(current))
                                                 for k, n, d in info.members if k == kind and n == name})
        results.append(result)
    return results


def validate_release_fixture(path, version):
    metadata = json.loads((path.parent / f"version-{version}.json").read_text(encoding="utf-8"))
    artifact = metadata["downloads"]["client"]
    if metadata["id"] != version or metadata["type"] != "release":
        raise ValueError("Expected exact " + version + " release metadata")
    with path.open("rb") as source:
        digest = hashlib.file_digest(source, "sha1").hexdigest()
    if path.stat().st_size != artifact["size"] or digest != artifact["sha1"]:
        raise ValueError(version + " client fixture failed Mojang size/SHA-1 validation")


def compare_classes(results, universe, baseline_path):
    """Compare class bytes for SDK owners plus all declaring parents/interfaces.

    This detects changes to the inspected code, not every class in Minecraft.
    A different body is a review signal, not an automatic ABI failure.
    """
    candidates = set()
    for record in results:
        candidates.update(universe.lineage(record["owner"]))
    with ClassArchive(baseline_path) as baseline:
        names = sorted(name for name in candidates if baseline.get(name))
        changed = [name for name in names if not universe.get(name) or
                   baseline.jar.read(name + ".class") != universe.archives[0].jar.read(name + ".class")]
    return {"baseline": "26.1", "client_classes_compared": len(names), "changed_classes": changed}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client", type=Path, required=True)
    parser.add_argument("--target", choices=TARGETS, default="26.1")
    parser.add_argument("--compare-client", type=Path, help="Optional verified 26.1 client jar for class-byte comparison")
    parser.add_argument("--library-dir", type=Path, required=True)
    parser.add_argument("--java-home", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--compiler", default=os.environ.get("CXX", "g++"))
    args = parser.parse_args()
    compiler = shutil.which(args.compiler)
    if not compiler: parser.error("GCC/Clang C++20 compiler required")
    universe = None
    try:
        validate_release_fixture(args.client, args.target)
        if args.compare_client: validate_release_fixture(args.compare_client, "26.1")
        universe = Universe([args.client, *sorted(args.library_dir.glob("*.jar"))], args.java_home)
        client = universe.get("net/minecraft/client/Minecraft")
        if not client or client.major != 69: raise ValueError("Expected Mojang-named Java 25 client")
        records = compile_contracts(compiler, TARGETS[args.target])
        sdk = inspect(records, universe)
        raw = inspect(raw_contracts(compiler, records, TARGETS[args.target]), universe)
        results = sdk + raw
        counts = dict(Counter(r["status"] for r in results))
        report = {"target": args.target, "scope": "all selected SDK declarations and reviewed raw JNI sites, not game behavior",
                  "sdk_count": len(sdk), "raw_count": len(raw), "counts": counts, "bindings": results}
        if args.compare_client:
            report["comparison"] = compare_classes(results, universe, args.compare_client)
            print("Client class comparison:", report["comparison"])
        args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print("SDK declaration audit:", counts)
        for result in results:
            if result["status"] != "present": print(result["status"], result["cpp"], result.get("alternatives", []))
        return 0 if all(r["status"] == "present" for r in results) else 1
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError, zipfile.BadZipFile) as error:
        parser.exit(2, f"SDK audit failed: {error}\n")
    finally:
        if universe: universe.close()


if __name__ == "__main__":
    raise SystemExit(main())
