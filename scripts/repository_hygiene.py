"""Read-only first-party file/link checks, not a dead-code detector."""
from pathlib import Path
import os
import re
import xml.etree.ElementTree as ET
from urllib.parse import unquote, urlsplit

AREAS = ("src", "config-service", "scripts", "tests", "docs", "assets", ".github")
SKIP_DIRS = {"node_modules", ".wrangler", ".credentials", "__pycache__", "archive"}
TEXT_TYPES = {".cpp", ".hpp", ".h", ".inl", ".java", ".js", ".mjs", ".py",
              ".ps1", ".md", ".sql", ".props", ".json", ".jsonc", ".yml",
              ".vcxproj", ".filters", ".slnx", ".rc", ".cmd"}
CPP_TYPES = {".cpp", ".hpp", ".h", ".inl"}


def first_party_files(root):
    # Never inspect vendor trees, generated state, vaults or local credentials.
    for path in sorted(root.iterdir()):
        if path.is_file() and path.suffix in TEXT_TYPES:
            yield path
    for area in AREAS:
        for directory, children, files in os.walk(root / area):
            children[:] = sorted(name for name in children if name not in SKIP_DIRS)
            for name in sorted(files):
                path = Path(directory) / name
                if path.suffix in TEXT_TYPES and not name.startswith((".env", ".dev.vars")):
                    yield path


def markdown_anchors(source):
    anchors, seen, fenced = set(), {}, False
    for line in source.splitlines():
        if line.lstrip().startswith(("```", "~~~")):
            fenced = not fenced
        if fenced:
            continue
        match = re.match(r"^#{1,6}\s+(.+?)\s*#*\s*$", line)
        if match:
            slug = re.sub(r"[^\w -]", "", match[1].lower()).replace(" ", "-")
            count = seen.get(slug, 0)
            seen[slug] = count + 1
            anchors.add(slug + (f"-{count}" if count else ""))
    return anchors


def validate_markdown(path):
    errors = []
    source = path.read_text(encoding="utf-8")
    # Inline file/image links only; external links and generated examples are not fetched.
    source = re.sub(r"```.*?```|~~~.*?~~~", "", source, flags=re.S)
    for target in re.findall(r"\[[^\]\n]*\]\(([^)\n]+)\)", source):
        target = target.strip().strip("<>")
        parts = urlsplit(target)
        if parts.scheme or parts.netloc:
            continue
        destination = path.parent / unquote(parts.path) if parts.path else path
        if not destination.exists():
            errors.append(f"Broken Markdown link: {path}: {target}")
        elif parts.fragment and destination.suffix == ".md" and destination.is_file():
            if unquote(parts.fragment) not in markdown_anchors(destination.read_text(encoding="utf-8")):
                errors.append(f"Missing Markdown heading: {path}: {target}")
    return errors


def validate_resources(root):
    errors, entries = [], []
    resources, header = root / "assets.rc", root / "resource.h"
    if not resources.is_file() or not header.is_file():
        return ["Missing assets.rc or resource.h"]
    definitions = dict(re.findall(r"^\s*#define\s+(\w+)\s+(\d+)\s*$",
                                  header.read_text(encoding="utf-8"), re.M))
    for name, filename in re.findall(r'^\s*(\w+)\s+RCDATA\s+"([^"]+)"',
                                     resources.read_text(encoding="utf-8"), re.M):
        target = root / filename.replace("\\", "/")
        if not target.is_file():
            errors.append(f"Missing resource file: {name}: {filename}")
        if name not in definitions:
            errors.append(f"Undefined resource ID: {name}")
        identity = definitions.get(name, name)
        if identity in entries:
            errors.append(f"Duplicate resource ID: {name}")
        entries.append(identity)
    return errors


def validate_build_file(path):
    errors = []
    for node in ET.parse(path).iter():
        kind = node.tag.rsplit("}", 1)[-1]
        target = node.get("Path") if kind == "Project" else (
            node.get("Include") if kind in {"ClCompile", "ClInclude", "ResourceCompile", "ProjectReference"} else None)
        # MSBuild evaluates variables and configuration conditions. Only literal
        # file references are checked here; this does not replace a native build.
        if not target or "$" in target or "%" in target:
            continue
        normalized = target.replace("\\", "/")
        matches = list(path.parent.glob(normalized)) if any(c in normalized for c in "*?") else [path.parent / normalized]
        if not matches or any(not match.is_file() for match in matches):
            errors.append(f"Missing build file: {path}: {target}")
    return errors


def validate_repository(root):
    root = Path(root).resolve()
    errors = validate_resources(root)
    include_roots = [root / directory for directory in (
        "src/modules", "include", "include/imgui", "include/jni", "include/jni/win32",
        "include/glm", "include/minhook")]
    for path in first_party_files(root):
        source = path.read_text(encoding="utf-8-sig")
        if not source.strip():
            errors.append(f"Empty first-party text file: {path.relative_to(root)}")
        if path.suffix in CPP_TYPES:
            for include in re.findall(r'^\s*#\s*include\s*"([^"]+)"', source, re.M):
                if not any((base / include).is_file() for base in [path.parent, *include_roots]):
                    errors.append(f"Broken quoted include: {path.relative_to(root)}: {include}")
        elif path.suffix in {".js", ".mjs"}:
            imports = re.findall(r'''(?:\bfrom\s*|\bimport\s*\(?\s*)['"](\.[^'"]+)['"]''', source)
            for dependency in imports:
                if not (path.parent / dependency).is_file():
                    errors.append(f"Broken JS import: {path.relative_to(root)}: {dependency}")
        elif path.suffix == ".md":
            errors.extend(validate_markdown(path))
        elif path.suffix in {".slnx", ".vcxproj"}:
            errors.extend(validate_build_file(path))
    return errors
