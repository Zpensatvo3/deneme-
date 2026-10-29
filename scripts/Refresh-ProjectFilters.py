from pathlib import Path
import uuid
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parent.parent
PROJECT_PATH = ROOT / "OneStep.vcxproj"
FILTERS_PATH = ROOT / "OneStep.vcxproj.filters"
NAMESPACE = "http://schemas.microsoft.com/developer/msbuild/2003"
ET.register_namespace("", NAMESPACE)


def tag(name: str) -> str:
    return f"{{{NAMESPACE}}}{name}"


def assigned_filter(kind: str, include: str) -> str:
    normalized = include.replace("/", "\\")
    parts = normalized.split("\\")
    parent = "\\".join(parts[:-1])
    if normalized.startswith("src\\"):
        base = "Source Files" if kind == "ClCompile" else "Header Files"
        return base + ("\\" + parent if parent else "")
    if normalized.startswith("include\\"):
        group = "\\".join(parts[:3] if len(parts) > 2 else parts[:-1])
        return "Third Party" + ("\\" + group if group else "")
    if kind == "ResourceCompile" or normalized.lower().endswith((".rc", ".ico", ".png", ".bin")):
        return "Resource Files"
    return "Header Files" if kind == "ClInclude" else "Source Files"


source = ET.parse(PROJECT_PATH).getroot()
items: list[tuple[str, str, str]] = []
filters = {"Source Files", "Header Files", "Resource Files", "Third Party"}
for item_kind in ("ClCompile", "ClInclude", "ResourceCompile", "None"):
    for node in source.iter(tag(item_kind)):
        include = node.get("Include")
        if not include:
            continue
        destination = assigned_filter(item_kind, include)
        items.append((item_kind, include, destination))
        pieces = destination.split("\\")
        filters.update("\\".join(pieces[:index]) for index in range(1, len(pieces) + 1))

project = ET.Element(tag("Project"), {"ToolsVersion": "4.0"})
filter_group = ET.SubElement(project, tag("ItemGroup"))
for name in sorted(filters, key=lambda value: (value.count("\\"), value.lower())):
    node = ET.SubElement(filter_group, tag("Filter"), {"Include": name})
    identifier = ET.SubElement(node, tag("UniqueIdentifier"))
    identifier.text = "{" + str(uuid.uuid5(uuid.NAMESPACE_URL, "thedorius-filter:" + name)).upper() + "}"

for item_kind in ("ClCompile", "ClInclude", "ResourceCompile", "None"):
    matching = [item for item in items if item[0] == item_kind]
    if not matching:
        continue
    group = ET.SubElement(project, tag("ItemGroup"))
    for _, include, destination in matching:
        node = ET.SubElement(group, tag(item_kind), {"Include": include})
        ET.SubElement(node, tag("Filter")).text = destination

ET.indent(project, space="  ")
xml = ET.tostring(project, encoding="unicode", xml_declaration=False)
FILTERS_PATH.write_text('<?xml version="1.0" encoding="utf-8"?>\n' + xml + "\n", encoding="utf-8")
print(f"Updated {FILTERS_PATH.name}: {len(items)} items, {len(filters)} filters")
