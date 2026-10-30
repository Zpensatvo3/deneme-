"""Run the Auto Totem controller and generated non-clickable inventory in an isolated JVM."""
from pathlib import Path
import argparse
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--asm-classpath", required=True)
args = parser.parse_args()
java, javac = shutil.which("java"), shutil.which("javac")
if not java or not javac:
    raise SystemExit("JDK 17+ is required")
with tempfile.TemporaryDirectory(prefix="onestep-totem-") as work:
    classes = Path(work) / "classes"
    sources = [ROOT / "src/bridge" / (name + ".java") for name in ("WorldBridge", "ClientTickTransformer", "ClientTickRelay")]
    sources += sorted((ROOT / "tests/auto_totem").glob("*.java"))
    subprocess.run([javac, *[str(ROOT / "src/bridge" / (name + ".java")) for name in ("Json", "ModuleCatalog", "ModuleManager", "ConfigManager", "ManagerBridge", "GameBridge", "UtilityBridge", "ItemBridge", "InventoryBridge", "WorldBridge", "ClientTickRelay")], str(ROOT / "tests/support/ManagerTestDriver.java"), "--release", "17", "-encoding", "UTF-8", "-g:none", "-cp", args.asm_classpath,
                    "-d", str(classes), *map(str, sources)], check=True)
    for mode, roots in (("source", (classes,)), ("embedded", (ROOT / "assets/hybrid", classes))):
        cp = os.pathsep.join((*map(str, roots), args.asm_classpath))
        subprocess.run([java, "-Xverify:all", "-cp", cp, "net.minecraft.AutoTotemBridgeTest", mode], cwd=ROOT, check=True)
