"""Small geometry check and optional QA preview of the generated vector asset."""
import argparse
import importlib.util
from pathlib import Path
import re
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preview", type=Path)
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location("spotify_compiler", ROOT / "scripts/Build-SpotifyVector.py")
    compiler = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(compiler)
    xml = ET.fromstring(compiler.SOURCE.read_text(encoding="utf-8"))
    rings = compiler.contours(xml.find("{http://www.w3.org/2000/svg}path").attrib["d"])
    mesh = compiler.OUTPUT.read_text(encoding="utf-8").split("triangles{{", 1)[1].split("}};", 1)[0]
    points = [tuple(map(float, pair)) for pair in re.findall(r"\{([-0-9.]+)f, ([-0-9.]+)f\}", mesh)]
    triangles = [points[i:i + 3] for i in range(0, len(points), 3)]
    def filled(point, triangle):
        cross = [((b[0] - a[0]) * (point[1] - a[1]) - (b[1] - a[1]) * (point[0] - a[0])) for a, b in zip(triangle, triangle[1:] + triangle[:1])]
        return all(v >= -1e-10 for v in cross) or all(v <= 1e-10 for v in cross)
    for y in range(64):
        py = (y + .37) / 64
        row = [t for t in triangles if min(p[1] for p in t) <= py <= max(p[1] for p in t)]
        for x in range(64):
            p = ((x + .31) / 64, py)
            svg_point = (p[0] * 2931, p[1] * 2931)
            expected = compiler.inside(svg_point, rings[0]) and not any(compiler.inside(svg_point, hole) for hole in rings[1:])
            assert any(filled(p, t) for t in row) == expected, ("SVG/mesh mismatch", p)
    print("PASS SVG outline and three transparent cutouts match vector fill")
    if args.preview:
        from PIL import Image, ImageDraw
        scale = 4
        result = Image.new("RGB", (720 * scale, 270 * scale), "#111016")
        draw = ImageDraw.Draw(result)
        for left, top, size in ((28, 50, 28), (100, 42, 48), (210, 28, 96), (390, 24, 210)):
            for triangle in triangles:
                draw.polygon([((left + p[0] * size) * scale, (top + p[1] * size) * scale) for p in triangle], fill="#2ebd59")
        args.preview.parent.mkdir(parents=True, exist_ok=True)
        result.resize((720, 270), Image.Resampling.LANCZOS).save(args.preview)
        print("Saved vector geometry QA preview")


if __name__ == "__main__":
    main()
