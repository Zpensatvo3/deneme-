"""Check source and embedded Java bridges against local Mojang/Fabric fixtures.

Requires a JDK (25 for 26.1), ASM 9.9 core/tree/commons/analysis and JOML 1.10.8.
No downloads, running game, native registration, OpenGL, DLL build or remote writes.
"""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
TARGETS = {"1.21.11": 12111,
           "26.1": 26100, "26.1.1": 26101, "26.1.2": 26102}
BRIDGES = ("SkyBridge", "SkyFogBridge", "SkyTransformer")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture-dir", type=Path, required=True, help="client-<version>.jar and intermediary-<1.21.version>.jar")
    parser.add_argument("--library-dir", type=Path, required=True, help="ASM/JOML jars and fabric-rendering-<version>.jar")
    parser.add_argument("--java-home", type=Path, default=os.environ.get("JAVA_HOME"))
    parser.add_argument("--target", choices=TARGETS, action="append", help="Repeat to limit the tested releases; default all four")
    args = parser.parse_args()
    if args.java_home: args.java_home = args.java_home.resolve()
    targets = tuple(dict.fromkeys(args.target or TARGETS))
    suffix = ".exe" if os.name == "nt" else ""
    java = str(args.java_home / "bin" / ("java" + suffix)) if args.java_home else shutil.which("java")
    javac = str(args.java_home / "bin" / ("javac" + suffix)) if args.java_home else shutil.which("javac")
    if not java or not javac: parser.error("A JDK with java and javac is required")
    try:
        runtime = subprocess.run([java, "-version"], capture_output=True, text=True, check=True)
        match = re.search(r'version "(\d+)', runtime.stdout + runtime.stderr)
        required = 25 if any(TARGETS[t] >= 26100 for t in targets) else 21
        if not match or int(match[1]) < required:
            parser.error(f"These classfiles require Java {required} or newer")
        libraries = [args.library_dir / (name + ".jar") for name in ("asm", "asm-tree", "asm-commons", "asm-analysis", "joml")]
        fixtures = []
        for target in targets:
            client = args.fixture_dir / f"client-{target}.jar"
            intermediary = args.fixture_dir / f"intermediary-{target}.jar"
            rendering = args.library_dir / f"fabric-rendering-{target}.jar"
            fixtures += [client, rendering] + ([] if TARGETS[target] >= 26100 else [intermediary])
        for file in libraries + fixtures:
            if not file.is_file(): parser.error(f"Missing local fixture: {file}")
        # Missing embedded resources must fail, rather than falling back to the
        # freshly compiled source later on the test JVM's classpath.
        for name in BRIDGES:
            package = "skybridge/net/onestep/visual"
            resource = ROOT / "assets" / package / (name + ".class")
            if not resource.is_file(): parser.error(f"Missing embedded bridge: {resource}")
        with tempfile.TemporaryDirectory(prefix="onestep-java-bridges-") as directory:
            classes = Path(directory) / "classes"
            library_cp = os.pathsep.join(str(p.resolve()) for p in libraries)
            sources = [ROOT / "src/bridge" / (name + ".java") for name in BRIDGES]
            sources += [ROOT / "tests/BridgeCompatibilityTest.java", ROOT / "tests/SkyBridgeInstallationTest.java"]
            subprocess.run([javac, "--release", "17", "-encoding", "UTF-8", "-g:none", "-cp", library_cp,
                            "-d", str(classes), *map(str, sources)], cwd=directory, check=True)
            for mode in ("source", "embedded"):
                classpath = [classes] if mode == "source" else [ROOT / "assets/skybridge", classes]
                cp = os.pathsep.join([*map(str, classpath), library_cp])
                for target in targets:
                    print(f"Checking {mode} bridge resources for {target}", flush=True)
                    subprocess.run([java, "-ea", "-Xverify:all", "-cp", cp, "BridgeCompatibilityTest",
                                    str((args.fixture_dir / f"client-{target}.jar").resolve()),
                                    "-" if TARGETS[target] >= 26100 else str((args.fixture_dir / f"intermediary-{target}.jar").resolve()),
                                    str(TARGETS[target]), str((args.library_dir / f"fabric-rendering-{target}.jar").resolve())], check=True)
            print(f"PASS: {len(targets)} exact targets, source and embedded bridges; no game or native renderer executed")
        return 0
    except (OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Bridge validation failed: {error}\n")


if __name__ == "__main__":
    raise SystemExit(main())
