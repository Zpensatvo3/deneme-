"""Regressions for missing Java entry points that disable a shared JNI bridge."""
from pathlib import Path
import runpy
import shutil
import tempfile

ROOT = Path(__file__).resolve().parent.parent
api = runpy.run_path(str(ROOT / "scripts/Check-GameBridgeContracts.py"))
validate, bridges = api["validate"], api["BRIDGES"]
classes = ROOT / "assets/hybrid"
errors, count = validate(classes)
assert not errors, errors
checks = 1
with tempfile.TemporaryDirectory(prefix="onestep-bridge-contracts-") as directory:
    copied = Path(directory) / "classes"
    shutil.copytree(classes, copied)
    world = copied / "net/onestep/hybrid/WorldBridge.class"
    original = world.read_bytes()
    for method, renamed in ((b"triggerService", b"triggerServicX"), (b"tickTrigger", b"tickTriggeX")):
        assert method in original and len(method) == len(renamed)
        world.write_bytes(original.replace(method, renamed))
        failures, _ = validate(copied)
        assert any(method.decode() in error for error in failures), failures
        checks += 1
    world.write_bytes(original)
    native_root = Path(directory) / "native"
    native_dir = native_root / "src/services/game"
    native_dir.mkdir(parents=True)
    for source, _ in bridges.values():
        shutil.copy2(ROOT / "src/services/game" / source, native_dir / source)
    native_world = native_dir / "world_bridge.cpp"
    text = native_world.read_text(encoding="utf-8")
    old = '(ILjava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;IJ)J'
    assert old in text
    native_world.write_text(text.replace(old, '(ILjava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;I)J'), encoding="utf-8")
    failures, _ = validate(copied, root=native_root)
    assert any("triggerService" in error and "RegisterNatives" in error for error in failures), failures
    checks += 1
    native_world.write_text(text, encoding="utf-8")
    native_item = native_dir / "item_bridge.cpp"
    native_item.write_text(native_item.read_text(encoding="utf-8") +
        '\nenv->GetMethodID(item, "missingMethod", "()[Ljava/lang/Object;");\n', encoding="utf-8")
    failures, _ = validate(copied, root=native_root)
    assert any("ItemBridge" in error and "missingMethod" in error for error in failures), failures
    checks += 1
print(f"PASS: {checks} bridge contract regressions; {count} live native/Java contracts")
