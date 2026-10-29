"""Check JNI method contracts against the classfiles that are actually packaged.

Reads files only; does not attach to Minecraft or execute bridge game actions.
"""
import argparse
from pathlib import Path
import re
import struct
from minecraft_classfile import read_class

ROOT = Path(__file__).resolve().parent.parent
BRIDGES = {
    "GameBridge": ("hybrid_bridge.cpp", "type"),
    "UtilityBridge": ("utility_bridge.cpp", "utility"),
    "ItemBridge": ("item_bridge.cpp", "item"),
    "InventoryBridge": ("inventory_bridge.cpp", "type"),
    "WorldBridge": ("world_bridge.cpp", "type"),
}


def pe_resource(data, resource_id):
    """Read RCDATA without loading/executing the DLL."""
    def u16(offset): return struct.unpack_from("<H", data, offset)[0]
    def u32(offset): return struct.unpack_from("<I", data, offset)[0]
    pe = u32(0x3c)
    if data[pe:pe + 4] != b"PE\0\0": raise ValueError("Not a PE image")
    optional = pe + 24
    sections = optional + u16(pe + 20)

    def file_offset(rva):
        for index in range(u16(pe + 6)):
            section = sections + index * 40
            virtual_size, virtual_address, size, raw = struct.unpack_from("<IIII", data, section + 8)
            if virtual_address <= rva < virtual_address + max(virtual_size, size):
                return raw + rva - virtual_address
        raise ValueError(f"Unmapped PE address {rva:x}")

    directories = optional + (112 if u16(optional) == 0x20b else 96)
    base = file_offset(u32(directories + 16))

    def entry(relative, wanted=None):
        directory = base + relative
        for index in range(u16(directory + 12) + u16(directory + 14)):
            key, value = struct.unpack_from("<II", data, directory + 16 + index * 8)
            if wanted is None or key == wanted: return value & 0x7fffffff
        raise ValueError(f"Missing PE resource {resource_id}")

    leaf = base + entry(entry(entry(0, 10), resource_id))
    address, size = struct.unpack_from("<II", data, leaf)
    offset = file_offset(address)
    blob = data[offset:offset + size]
    if len(blob) != size: raise ValueError("Truncated PE resource")
    return blob


def resource_ids(root=ROOT):
    return {name: int(number) for name, number in re.findall(
        r"#define\s+(IDR_\w+)\s+(\d+)", (root / "resource.h").read_text(encoding="utf-8"))}


def validate(classes, dll=None, root=ROOT):
    errors, checked = [], 0
    blobs = {}
    for name in BRIDGES:
        path = classes / "net/onestep/hybrid" / (name + ".class")
        blobs[name] = path.read_bytes()
    if dll is not None:
        image = dll.read_bytes()
        ids = resource_ids(root)
        resources = re.findall(r'(IDR_\w+)\s+RCDATA\s+"(assets[^"\n]+\.class)"',
                               (root / "assets.rc").read_text(encoding="utf-8"))
        for identifier, raw_path in resources:
            path = Path(raw_path.replace("\\", "/"))
            if "hybrid" not in path.parts: continue
            expected = (classes / "net/onestep/hybrid" / path.name).read_bytes()
            actual = pe_resource(image, ids[identifier])
            if actual != expected: errors.append(f"{dll.name}: stale embedded {path.name}")
            if path.stem in blobs: blobs[path.stem] = actual
    for name, (source, variable) in BRIDGES.items():
        info = read_class(blobs[name])
        if info.name != "net/onestep/hybrid/" + name:
            errors.append(f"{name}: incorrect class resource {info.name}")
        text = (root / "src/services/game" / source).read_text(encoding="utf-8")
        methods = re.findall(r'GetMethodID\(\s*' + re.escape(variable) +
                             r'\s*,\s*"([^"\n]+)"\s*,\s*"([^"\n]+)"\s*\)', text)
        if not methods: errors.append(f"{source}: no bridge method contracts found")
        for method, descriptor in methods:
            checked += 1
            flags = info.members.get(("method", method, descriptor))
            if flags is None or flags & 0x8:
                errors.append(f"{name}: missing instance method {method}{descriptor}")
        callbacks = re.findall(r'\{const_cast<char\*>\("([^"\n]+)"\),\s*'
                               r'const_cast<char\*>\("([^"\n]+)"\)', text)
        registered = set(callbacks)
        natives = {(method, descriptor) for (kind, method, descriptor), flags in info.members.items()
                   if kind == "method" and flags & 0x100}
        checked += len(registered)
        for method, descriptor in sorted(registered - natives):
            errors.append(f"{name}: RegisterNatives requires missing native {method}{descriptor}")
        for method, descriptor in sorted(natives - registered):
            errors.append(f"{name}: native {method}{descriptor} has no registration")
    return errors, checked


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--classes", type=Path, default=ROOT / "assets/hybrid")
    parser.add_argument("--dll", type=Path)
    args = parser.parse_args()
    errors, count = validate(args.classes, args.dll)
    if errors:
        for error in errors: print("ERROR:", error)
        raise SystemExit(1)
    print(f"PASS: {count} JNI method/native contracts across {len(BRIDGES)} game bridges" +
          ("; packaged DLL resources match" if args.dll else ""))


if __name__ == "__main__": main()
