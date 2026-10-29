"""Compile the supplied Spotify SVG into an original, texture-free ImGui mesh.

This deliberately accepts only this asset's M/L/C/S/Z path grammar. No SVG scripts,
external resources, third-party tessellator or runtime SVG parser is involved.
Generated header is a mechanical asset conversion, not a hand-redrawn logo.
"""
from pathlib import Path
import argparse
import hashlib
import math
import re
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "assets/icons/spotify.svg"
OUTPUT = ROOT / "src/ui/spotify_vector_data.hpp"


def midpoint(a, b):
    return ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2)


def flatten(a, b, c, d, output, depth=0):
    dx, dy = d[0] - a[0], d[1] - a[1]
    length = math.hypot(dx, dy)
    distance = lambda p: abs(dx * (p[1] - a[1]) - dy * (p[0] - a[0])) / length if length else math.dist(p, a)
    if max(distance(b), distance(c)) <= 1.5 or depth >= 16:
        output.append(d)
        return
    ab, bc, cd = midpoint(a, b), midpoint(b, c), midpoint(c, d)
    abc, bcd = midpoint(ab, bc), midpoint(bc, cd)
    center = midpoint(abc, bcd)
    flatten(a, ab, abc, center, output, depth + 1)
    flatten(center, bcd, cd, d, output, depth + 1)


def contours(path):
    tokens = re.findall(r"[A-Za-z]|[-+]?(?:\d*\.\d+|\d+\.?\d*)(?:[eE][-+]?\d+)?", path)
    cursor = 0
    current, previous, command = (0., 0.), None, None
    result, ring = [], []
    while cursor < len(tokens):
        if tokens[cursor].isalpha():
            command = tokens[cursor]
            cursor += 1
        upper, relative = command.upper(), command.islower()
        if upper == "Z":
            if ring[-1] == ring[0]:
                ring.pop()
            if len(ring) < 3:
                raise ValueError("Degenerate contour")
            result.append(ring)
            current, previous, ring, command = ring[0], None, [], None
            continue
        size = {"M": 2, "L": 2, "C": 6, "S": 4}.get(upper)
        if not size:
            raise ValueError(f"Unsupported path command: {command}")
        values = list(map(float, tokens[cursor:cursor + size]))
        if len(values) != size or not all(math.isfinite(v) for v in values):
            raise ValueError("Invalid path data")
        cursor += size
        points = [(values[i] + (current[0] if relative else 0), values[i + 1] + (current[1] if relative else 0)) for i in range(0, size, 2)]
        if upper in ("M", "L"):
            current = points[0]
            ring.append(current)
            previous = None
            if upper == "M":
                command = "l" if relative else "L"
        else:
            if upper == "S":
                control = (2 * current[0] - previous[0], 2 * current[1] - previous[1]) if previous else current
                points.insert(0, control)
            flatten(current, *points, ring)
            previous, current = points[1], points[2]
    if ring:
        raise ValueError("Unclosed path")
    return result


def inside(point, polygon):
    x, y = point
    hit = False
    for a, b in zip(polygon, polygon[1:] + polygon[:1]):
        if (a[1] > y) != (b[1] > y) and x < a[0] + (y - a[1]) * (b[0] - a[0]) / (b[1] - a[1]):
            hit = not hit
    return hit


def compile_svg():
    root = ET.fromstring(SOURCE.read_text(encoding="utf-8"))
    if any(e.tag.rsplit("}", 1)[-1] not in ("svg", "style", "path") for e in root.iter()):
        raise ValueError("This compiler accepts only the static supplied path asset")
    paths = root.findall("{http://www.w3.org/2000/svg}path")
    if len(paths) != 1 or root.attrib["viewBox"] != "0 0 2931 2931":
        raise ValueError("Unexpected Spotify asset")
    path = paths[0].attrib["d"]
    color = re.search(r"fill:\s*#([a-fA-F0-9]{6})", "".join(root.itertext())).group(1)
    rings = contours(path)
    if len(rings) != 4:
        raise ValueError("Expected outline and three transparent wave cutouts")
    edges = [(a, b) for ring in rings for a, b in zip(ring, ring[1:] + ring[:1]) if a[1] != b[1]]
    levels = sorted({p[1] for ring in rings for p in ring})
    triangles = []
    def at(edge, y):
        a, b = edge
        return (a[0] + (y - a[1]) * (b[0] - a[0]) / (b[1] - a[1]), y)
    for low, high in zip(levels, levels[1:]):
        if high - low < 1e-8:
            continue
        middle = (low + high) / 2
        crossings = sorted((edge for edge in edges if min(edge[0][1], edge[1][1]) < middle < max(edge[0][1], edge[1][1])), key=lambda e: at(e, middle)[0])
        winding, left = 0, None
        for edge in crossings:
            before = winding
            winding += 1 if edge[1][1] > edge[0][1] else -1
            if before == 0 and winding:
                left = edge
            elif before and winding == 0:
                quad = [at(left, low), at(edge, low), at(edge, high), at(left, high)]
                for tri in ((quad[0], quad[1], quad[2]), (quad[0], quad[2], quad[3])):
                    a, b, c = tri
                    if abs((b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])) > 1e-7:
                        triangles.extend(tri)
        if winding:
            raise ValueError("Unbalanced SVG winding")
    outline, offsets = [], [0]
    for ring in rings:
        area = sum(a[0] * b[1] - b[0] * a[1] for a, b in zip(ring, ring[1:] + ring[:1]))
        depth = sum(inside(ring[0], other) for other in rings if other is not ring)
        sign = (1 if area > 0 else -1) * (-1 if depth % 2 else 1)
        normals = []
        for a, b in zip(ring, ring[1:] + ring[:1]):
            dx, dy = b[0] - a[0], b[1] - a[1]
            length = math.hypot(dx, dy)
            normals.append((sign * dy / length, -sign * dx / length))
        for i, point in enumerate(ring):
            a, b = normals[i - 1], normals[i]
            denominator = max(.05, 1 + a[0] * b[0] + a[1] * b[1])
            nx, ny = (a[0] + b[0]) / denominator, (a[1] + b[1]) / denominator
            length = math.hypot(nx, ny)
            if length > 2:
                nx, ny = nx * 2 / length, ny * 2 / length
            outline.append((*point, nx, ny))
        offsets.append(len(outline))
    number = lambda v: f"{v:.9f}f"
    lines = ["// Generated by scripts/Build-SpotifyVector.py from the user-supplied SVG; do not hand edit.",
             f"// Source path SHA256: {hashlib.sha256(path.encode()).hexdigest()}", "#pragma once", "#include <array>",
             "namespace gui::spotify_vector_data {", "struct point { float x, y; };", "struct edge_point { float x, y, nx, ny; };",
             f"inline constexpr unsigned red = 0x{color[:2]}, green = 0x{color[2:4]}, blue = 0x{color[4:6]};",
             f"inline constexpr std::array<point, {len(triangles)}> triangles{{{{"]
    lines += ["    {" + number(p[0] / 2931) + ", " + number(p[1] / 2931) + "}," for p in triangles]
    lines += ["}};", f"inline constexpr std::array<edge_point, {len(outline)}> outline{{{{"]
    lines += ["    {" + ", ".join(map(number, (x / 2931, y / 2931, nx, ny))) + "}," for x, y, nx, ny in outline]
    lines += ["}};", "inline constexpr std::array<unsigned, 5> contours{" + ", ".join(map(str, offsets)) + "};", "}", ""]
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--output", type=Path, default=OUTPUT)
    args = parser.parse_args()
    generated = compile_svg()
    if args.check:
        if not args.output.is_file() or args.output.read_text(encoding="utf-8") != generated:
            raise SystemExit("Spotify mesh is stale; run scripts/Build-SpotifyVector.py")
        print("PASS supplied SVG matches compiled vector mesh")
    else:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(generated, encoding="utf-8", newline="\n")
        print("Generated exact-path Spotify vector mesh")


if __name__ == "__main__":
    main()
