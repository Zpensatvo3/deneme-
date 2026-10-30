"""Compile and exercise the shipping Java management core without a Minecraft runtime."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import importlib.util

ROOT = Path(__file__).resolve().parent.parent


def main():
    javac = shutil.which("javac")
    if not javac:
        raise SystemExit("JDK 17+ required (javac not found)")
    java = str(Path(javac).resolve().with_name("java.exe" if os.name == "nt" else "java"))
    sources = [ROOT / "src/bridge" / f"{name}.java" for name in
               ("Json", "ModuleCatalog", "ModuleManager", "ConfigManager", "ManagerBridge")]
    with tempfile.TemporaryDirectory(prefix="wolf-managers-") as directory:
        subprocess.run([javac, "--release", "17", "-encoding", "UTF-8", "-g:none", "-d", directory,
                        *map(str, sources), str(ROOT / "tests/ManagerCoreTest.java")], check=True)
        subprocess.run([java, "-ea", "-cp", directory, "net.onestep.hybrid.ManagerCoreTest"], check=True)
        subprocess.run([java, "-ea", "-cp", os.pathsep.join((str(ROOT / "assets/hybrid"), directory)),
                        "net.onestep.hybrid.ManagerCoreTest"], check=True)
        spec = importlib.util.spec_from_file_location("pack_manager", ROOT / "scripts/Pack-ManagerCore.py")
        packer = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(packer)
        bundle = Path(directory) / "ManagerCore.bin"
        packer.pack(Path(directory), bundle)
        packer.catalog(Path(directory) / "module_manager_catalog.inc")
        packer.values(Path(directory) / "catalog_values.hpp")
        generated = [bundle, Path(directory) / "module_manager_catalog.inc", Path(directory) / "catalog_values.hpp"]
        stamps = [path.stat().st_mtime_ns for path in generated]
        packer.pack(Path(directory), bundle)
        packer.catalog(generated[1])
        packer.values(generated[2])
        assert stamps == [path.stat().st_mtime_ns for path in generated], "No-op generation changed output timestamps"
        compiler = shutil.which(os.environ.get("CXX", "cl" if os.name == "nt" else "g++"))
        if not compiler:
            raise SystemExit("C++20 compiler required for the real JNI manager integration test")
        jdk = Path(javac).resolve().parent.parent
        platform = "win32" if os.name == "nt" else "linux"
        jvm = jdk / ("bin/server/jvm.dll" if os.name == "nt" else "lib/server/libjvm.so")
        output = Path(directory) / ("manager_jni.exe" if os.name == "nt" else "manager_jni")
        msvc = Path(compiler).name.lower() in ("cl", "cl.exe", "clang-cl", "clang-cl.exe")
        flags = ["/nologo", "/std:c++20", "/EHsc", "/UNDEBUG", "/I" + str(jdk / "include"),
                 "/I" + str(jdk / "include" / platform), "/Fe:" + str(output), "/Fo:" + directory + "\\"] if msvc else [
                 "-std=c++20", "-UNDEBUG", "-pthread", "-I" + str(jdk / "include"), "-I" + str(jdk / "include" / platform), "-o", str(output)]
        subprocess.run([compiler, *flags, str(ROOT / "tests/public_config_test.cpp"),
                        str(ROOT / "src/modules/core/manager.cpp"), str(ROOT / "src/modules/core/public_config.cpp"),
                        str(ROOT / "src/modules/core/config_fields.cpp"), *([] if os.name == "nt" else ["-ldl"])], check=True)
        subprocess.run([str(output), str(jvm), str(bundle)], check=True)
        print("PASS reproducible manager bundle/catalog generation; unchanged outputs keep their timestamps")


if __name__ == "__main__":
    main()
