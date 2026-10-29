"""Compile the 26.1 development SDK and reject unknown exact targets.

No network access, JVM, Minecraft, MSVC DLL/link verification or production service.
Use Test-Minecraft26Readiness.py --native-261 separately with official fixtures.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default=os.environ.get("CXX", "cl" if os.name == "nt" else "g++"))
    parser.add_argument("--target", choices=("26.1", "26.1.1", "26.1.2"), default="26.1")
    args = parser.parse_args()
    compiler = shutil.which(args.compiler)
    if not compiler: parser.error("A C++20 compiler is required")
    msvc = Path(compiler).name.lower() in ("cl", "cl.exe", "clang-cl", "clang-cl.exe")
    includes = (ROOT / "include/jni", ROOT / "include/jni" / ("win32" if os.name == "nt" else "linux"))
    target_id = {"26.1": 26100, "26.1.1": 26101, "26.1.2": 26102}[args.target]
    common = [compiler] + (["/nologo", "/std:c++20", "/EHsc", "/UNDEBUG", f"/DONESTEP_MINECRAFT_VERSION={target_id}"] if msvc
                           else ["-std=c++20", "-pthread", "-UNDEBUG", f"-DONESTEP_MINECRAFT_VERSION={target_id}"])
    for include in includes: common += [f"/I{include}"] if msvc else ["-I", str(include)]
    try:
        with tempfile.TemporaryDirectory(prefix="onestep-261-offline-") as directory:
            work = Path(directory)
            output = work / ("sdk.exe" if os.name == "nt" else "sdk")
            command = common + ([f"/Fe:{output}"] if msvc else ["-o", str(output)])
            subprocess.run(command + [str(ROOT / "tests/minecraft261_sdk_test.cpp")], cwd=work, check=True)
            subprocess.run([str(output)], cwd=work, check=True)
            syntax = ["/Zs"] if msvc else ["-fsyntax-only"]
            for source in ("src/platform/adapter/game_adapter.cpp", "src/platform/adapter/item_model.cpp", "src/utils/version_manager.cpp"):
                subprocess.run(common + syntax + [str(ROOT / source)], cwd=work, check=True)
            for header in ("src/modules/visual/chams_model.hpp", "src/modules/visual/sky_bindings.hpp", "src/modules/vape/game_bindings.hpp"):
                probe = work / "header.cpp"
                probe.write_text(f'#include "{(ROOT / header).as_posix()}"\n', encoding="utf-8")
                subprocess.run(common + syntax + [str(probe)], cwd=work, check=True)
            probe = work / "guard.cpp"
            probe.write_text(f'#include "{(ROOT / "src/platform/adapter/target_version.hpp").as_posix()}"\n', encoding="utf-8")
            invalid = [arg for arg in common if "ONESTEP_MINECRAFT_VERSION=" not in arg]
            invalid += ["/DONESTEP_MINECRAFT_VERSION=26103"] if msvc else ["-DONESTEP_MINECRAFT_VERSION=26103"]
            rejected = subprocess.run(invalid + syntax + [str(probe)], cwd=work, capture_output=True, text=True)
            if rejected.returncode == 0 or "Unsupported OneStep Minecraft target" not in rejected.stdout + rejected.stderr:
                raise ValueError("Unknown exact target was not rejected")
            print("PASS: native adapter/item-model/version syntax and private binding headers")
            print(f"PASS: exact {args.target} compiles without porting bypass; unknown target 26103 rejected")
        return 0
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"26.1 offline tests failed: {error}\n")


if __name__ == "__main__":
    raise SystemExit(main())
