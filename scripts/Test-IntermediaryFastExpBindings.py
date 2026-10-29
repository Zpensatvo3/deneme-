"""Check Fast EXP bindings against a local 1.21.11 client and intermediary map."""
import argparse
from pathlib import Path
from minecraft_classfile import ClassArchive, read_intermediary

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--client", type=Path, required=True)
parser.add_argument("--intermediary", type=Path, required=True)
args = parser.parse_args()
classes, members = read_intermediary(args.intermediary)
owners = {intermediary: official for official, intermediary in classes.items()}
checks = [
    ("class_310", "field", "field_1700", "Lnet/minecraft/class_310;", True),
    ("class_310", "field", "field_1724", "Lnet/minecraft/class_746;", False),
    ("class_310", "field", "field_1687", "Lnet/minecraft/class_638;", False),
    ("class_310", "field", "field_1755", "Lnet/minecraft/class_437;", False),
    ("class_310", "field", "field_1752", "I", False),
    ("class_310", "method", "method_18854", "()Z", False),
    ("class_310", "method", "method_1583", "()V", False),
    ("class_746", "method", "method_5805", "()Z", False),
    ("class_746", "method", "method_6047", "()Lnet/minecraft/class_1799;", False),
    ("class_746", "method", "method_6079", "()Lnet/minecraft/class_1799;", False),
    ("class_1799", "method", "method_7960", "()Z", False),
    ("class_1799", "method", "method_7909", "()Lnet/minecraft/class_1792;", False),
    ("class_1802", "field", "field_8287", "Lnet/minecraft/class_1792;", True),
]
with ClassArchive(args.client) as client:
    for owner, kind, name, descriptor, is_static in checks:
        runtime_owner = owners.get("net/minecraft/" + owner)
        candidates = members.get((kind, name, descriptor), [])
        matches = []
        for _, runtime_name, runtime_descriptor in candidates:
            result = client.find(runtime_owner, kind, runtime_name, runtime_descriptor) if runtime_owner else None
            if result is not None and bool(result[1] & 0x0008) == is_static:
                matches.append(result)
        if not matches:
            raise SystemExit(f"Missing or incompatible binding: {owner}.{name}{descriptor}")
print(f"PASS {args.client.name}: {len(checks)} intermediary Fast EXP bindings")
