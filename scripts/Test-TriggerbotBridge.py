"""Exercise the real Java Triggerbot controller with offline game/JNI fixtures."""
from pathlib import Path
import argparse
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-only", action="store_true")
    args = parser.parse_args()
    javac, java = shutil.which("javac"), shutil.which("java")
    if not javac or not java:
        parser.error("A JDK 17+ is required")
    jdk = Path(javac).resolve().parent.parent
    compiler = shutil.which("cl") if os.name == "nt" else (shutil.which("clang++") or shutil.which("g++"))
    if not compiler:
        parser.error("A C++ compiler is required (Windows: Developer PowerShell)")
    with tempfile.TemporaryDirectory(prefix="onestep-triggerbot-") as directory:
        work = Path(directory)
        library = work / ("trigger_service.dll" if os.name == "nt" else "libtrigger_service.so")
        source = ROOT / "tests/triggerbot/trigger_service.cpp"
        includes = [jdk / "include", jdk / "include" / ("win32" if os.name == "nt" else "linux")]
        if os.name == "nt":
            command = [compiler, "/nologo", "/LD", "/EHsc", *[f"/I{p}" for p in includes], str(source), "/link", f"/OUT:{library}"]
        else:
            command = [compiler, "-shared", "-fPIC", *[f"-I{p}" for p in includes], str(source), "-o", str(library)]
        subprocess.run(command, cwd=work, check=True)
        classes = work / "classes"
        subprocess.run([javac, *[str(ROOT / "src/bridge" / (name + ".java")) for name in ("Json", "ModuleCatalog", "ModuleManager", "ConfigManager", "ManagerBridge", "GameBridge", "UtilityBridge", "ItemBridge", "InventoryBridge", "WorldBridge", "ClientTickRelay")], str(ROOT / "tests/support/ManagerTestDriver.java"), "--release", "17", "-encoding", "UTF-8", "-g:none", "-d", str(classes),
                        str(ROOT / "src/bridge/WorldBridge.java"), str(ROOT / "src/bridge/ClientTickRelay.java"),
                        str(ROOT / "tests/hybrid/GLFW.java"),
                        str(ROOT / "tests/triggerbot/TriggerbotBridgeTest.java")], check=True)
        modes = [("source", str(classes))]
        if not args.source_only:
            modes.append(("embedded", os.pathsep.join(map(str, (ROOT / "assets/hybrid", classes)))))
        for mode, cp in modes:
            subprocess.run([java, "-Xcheck:jni", "-cp", cp, "net.minecraft.TriggerbotBridgeTest", mode, str(library)], cwd=ROOT, check=True)


if __name__ == "__main__":
    main()
