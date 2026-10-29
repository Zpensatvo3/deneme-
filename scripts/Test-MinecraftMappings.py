"""Check compiled target-specific JNI descriptors against locally downloaded Yarn v2 jars.

No Minecraft process/JVM, network access, production service or DLL is required.
This checks symbol/descriptor availability, not in-game behavior or class inheritance.
"""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import argparse
import os
import shutil
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parent.parent
TARGETS = {"1.21.11": 12111}

def symbols(path):
    with zipfile.ZipFile(path) as jar:
        text = jar.read("mappings/mappings.tiny").decode("utf-8")
    header, *lines = text.splitlines()
    if header.split("\t") != ["tiny", "2", "0", "intermediary", "named"]:
        raise ValueError(f"Expected intermediary/named Yarn v2: {path}")
    return {(parts[3], parts[2]) for line in lines
            if len(parts := line.split("\t")) >= 5 and parts[0] == "" and parts[1] in ("m", "f")}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mapping-dir", type=Path, required=True, help="Contains yarn-1.21.11.jar (v2 classifier)")
    parser.add_argument("--compiler", default=os.environ.get("CXX", "cl" if os.name == "nt" else "g++"))
    parser.add_argument("--jobs", type=int, default=2)
    args = parser.parse_args()
    compiler = shutil.which(args.compiler)
    if not compiler or args.jobs < 1: parser.error("A C++20 compiler and positive --jobs are required")
    msvc = Path(compiler).name.lower() in ("cl", "cl.exe", "clang-cl", "clang-cl.exe")
    with tempfile.TemporaryDirectory(prefix="onestep-mappings-") as directory:
        def check(item):
            version, target = item
            work = Path(directory) / version
            work.mkdir()
            output = work / ("bindings.exe" if os.name == "nt" else "bindings")
            command = [compiler]
            if msvc:
                command += ["/nologo", "/std:c++20", "/EHsc", f"/DONESTEP_MINECRAFT_VERSION={target}", f"/Fe:{output}"]
            else:
                command += ["-std=c++20", "-pthread", f"-DONESTEP_MINECRAFT_VERSION={target}", "-o", str(output)]
            for include in (ROOT / "include/jni", ROOT / "include/jni" / ("win32" if os.name == "nt" else "linux")):
                command += [f"/I{include}"] if msvc else ["-I", str(include)]
            built = subprocess.run(command + [str(ROOT / "tests/minecraft_bindings_dump.cpp")], cwd=work, capture_output=True, text=True)
            if built.returncode: return version, False, built.stdout + built.stderr
            ran = subprocess.run([str(output)], cwd=work, capture_output=True, text=True)
            if ran.returncode: return version, False, ran.stdout + ran.stderr
            available = symbols(args.mapping_dir / f"yarn-{version}.jar")
            required = {tuple(line.split("\t")) for line in ran.stdout.splitlines()}
            missing = sorted(required - available)
            if missing: return version, False, "Missing intermediary symbols/descriptors:\n" + "\n".join(map(str, missing))
            return version, True, f"{len(required)} compiled JNI symbols/descriptors match Yarn"
        with ThreadPoolExecutor(max_workers=args.jobs) as pool:
            results = list(pool.map(check, TARGETS.items()))
        for version, passed, detail in results:
            print(f"{'PASS' if passed else 'FAIL'} {version}: {detail}")
        return 0 if all(passed for _, passed, _ in results) else 1

if __name__ == "__main__":
    raise SystemExit(main())
