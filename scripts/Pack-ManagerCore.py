"""Pack the Java manager core for parent-loader JNI injection. No timestamps in output."""
import argparse
from pathlib import Path
import struct
import re
import json

ROOT = Path(__file__).resolve().parent.parent


def catalog(output: Path):
    source = (ROOT / "src/bridge/ModuleCatalog.java").read_text(encoding="utf-8")
    ids = re.findall(r'^\s*m\("([a-z0-9_]+)"', source, re.M)
    if len(ids) != 35 or len(set(ids)) != 35:
        raise SystemExit("ModuleCatalog must declare all 35 unique native factories")
    lines = ["// Generated from ModuleCatalog.java. Do not edit."]
    for identifier in ids:
        factory = "&modules::create_vape_aim" if identifier == "vape_aim_assist" else (
            "&create_module<modules::" + ("friend_manager" if identifier == "friends_manager" else identifier) + ">")
        lines.append(f'{{ "{identifier}", {factory} }},')
    content = "\n".join(lines) + "\n"
    output.parent.mkdir(parents=True, exist_ok=True)
    if not output.exists() or output.read_text(encoding="utf-8") != content:
        output.write_text(content, encoding="utf-8")


def values(output: Path):
    source = (ROOT / "src/bridge/ModuleCatalog.java").read_text(encoding="utf-8")
    lines = ["// Generated from ModuleCatalog.java. Do not edit.", "#pragma once", "#include <string>", "#include <vector>", "namespace modules::catalog {"]
    identifier = None
    abi_source = (ROOT / "src/bridge/ModuleManager.java").read_text(encoding="utf-8")
    abi = re.search(r'public static final int ABI = (\d+);', abi_source)
    if not abi:
        raise SystemExit("Missing canonical ModuleManager ABI")
    lines.append(f"inline constexpr int manager_abi = {abi[1]};")
    for line in source.splitlines():
        module = re.match(r'\s*m\("([a-z0-9_]+)"', line)
        if module:
            identifier = module[1]
            flags = re.match(r'\s*m\("[^"]+", "[^"]+", "[^"]+", (true|false), (true|false)', line)
            if flags:
                lines.append(f"inline constexpr bool {identifier}_enabled_default = {flags[1]};")
                lines.append(f"inline constexpr bool {identifier}_supports_keybind = {flags[2]};")
        field = re.match(r'\s*f\("([^"\n]+)", (\d+), ("(?:[^"\\]|\\.)*"), ([^,]+), ([^,]+), (null|"[^"]*")((?:, \d+)*)\)', line)
        if not field:
            if line.lstrip().startswith("f(" ):
                raise SystemExit("Unsupported ModuleCatalog field syntax: " + line.strip())
            continue
        key, kind, initial, low, high = field.groups()[:5]
        kind = int(kind)
        default = json.loads(json.loads(initial))
        prefix = f"{identifier}_{key}"
        cpp_type = "bool" if kind == 0 else "int" if kind in (3, 4, 7, 9) else "float"

        def literal(value):
            if isinstance(value, bool):
                return str(value).lower()
            if cpp_type == "int":
                return str(int(value))
            return repr(float(value)) + "f"

        if kind == 8:
            strings = ", ".join(json.dumps(item, ensure_ascii=True) for item in default)
            lines.append(f"inline const std::vector<std::string> {prefix}_default{{{strings}}};")
        elif kind == 9:
            lines.append(f"inline const std::vector<int> {prefix}_default{{{', '.join(map(literal, default))}}};")
        elif isinstance(default, list):
            lines.append(f"inline constexpr {cpp_type} {prefix}_default[]{{{', '.join(map(literal, default))}}};")
        else:
            lines.append(f"inline constexpr {cpp_type} {prefix}_default = {literal(default)};")
        if kind in (1, 2, 4, 6, 7, 9):
            lines.append(f"inline constexpr {cpp_type} {prefix}_minimum = {literal(float(low))};")
            lines.append(f"inline constexpr {cpp_type} {prefix}_maximum = {literal(float(high))};")
    lines.append("}")
    content = "\n".join(lines) + "\n"
    if not output.exists() or output.read_text(encoding="utf-8") != content:
        output.write_text(content, encoding="utf-8")

CORE = ("Json", "ModuleCatalog", "ModuleManager", "ConfigManager", "ManagerBridge")


def pack(classes: Path, output: Path):
    base = classes / "net/onestep/hybrid"
    files = sorted(p for p in base.glob("*.class") if p.stem.split("$")[0] in CORE)
    if any(not (base / f"{name}.class").is_file() for name in CORE):
        raise SystemExit("Missing Java manager class; compile all five core sources first")
    payload = bytearray(b"WMC1" + struct.pack(">I", len(files)))
    # Define nested classes before enclosing classes (no custom superclass dependencies).
    files.sort(key=lambda p: ("$" not in p.stem, p.name))
    for path in files:
        name = f"net/onestep/hybrid/{path.stem}".encode("ascii")
        data = path.read_bytes()
        payload.extend(struct.pack(">I", len(name)) + name + struct.pack(">I", len(data)) + data)
    output.parent.mkdir(parents=True, exist_ok=True)
    if not output.exists() or output.read_bytes() != payload:
        output.write_bytes(payload)
    return len(files)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--classes", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--stamp", type=Path, help="MSBuild freshness marker; data files retain their mtimes when unchanged")
    args = parser.parse_args()
    print(f"Manager core: {pack(args.classes, args.output)} classes")
    catalog(args.output.with_name("module_manager_catalog.inc"))
    values(args.output.with_name("catalog_values.hpp"))
    if args.stamp:
        args.stamp.touch()
