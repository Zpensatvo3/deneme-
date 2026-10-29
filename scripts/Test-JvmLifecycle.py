"""Run the native attach/JVMTI lifecycle in a disposable Linux JDK 25 VM."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--java-home", type=Path, required=True)
    parser.add_argument("--compiler", default=os.environ.get("CXX", "g++"))
    args = parser.parse_args()
    if os.name == "nt": parser.error("Use a Linux development environment for this standalone runner")
    compiler = shutil.which(args.compiler)
    if not compiler: parser.error("A C++20 compiler is required")
    jdk = args.java_home.resolve()
    try:
        with tempfile.TemporaryDirectory(prefix="onestep-jvm-lifecycle-") as directory:
            work = Path(directory)
            subprocess.run([str(jdk / "bin/javac"), "--release", "25", "-g:none", "-d", str(work),
                            str(ROOT / "tests/ReTransformFixture.java")], check=True)
            executable = work / "lifecycle"
            subprocess.run([compiler, "-std=c++20", "-pthread", "-UNDEBUG", "-DONESTEP_MINECRAFT_VERSION=26100",
                            "-I", str(ROOT / "include/jni"), "-I", str(ROOT / "include/jni/linux"),
                            str(ROOT / "tests/jvm_lifecycle_test.cpp"), str(ROOT / "src/jvmti/jvmti.cpp"),
                            "-L" + str(jdk / "lib/server"), "-Wl,-rpath," + str(jdk / "lib/server"), "-ljvm", "-o", str(executable)], check=True)
            subprocess.run([str(executable), str(work / "net/onestep/test/ReTransformFixture.class")], check=True)
        return 0
    except (OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"JVM lifecycle failed: {error}\n")


if __name__ == "__main__":
    raise SystemExit(main())
