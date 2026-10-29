"""Aim Assist source checks only; no compiler, executable or DLL invocation."""
from pathlib import Path
import argparse
import re
import sys

ROOT = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--parser-dir", type=Path)
options = parser.parse_args()
sources = [
    "src/modules/combat/legit_aim_math.hpp", "src/modules/combat/legit_aim_config.hpp",
    "src/modules/combat/legit_aim_engine.hpp", "src/modules/combat/legit_aim_engine.cpp",
    "src/modules/combat/legit_click_aim_assist.hpp", "src/modules/combat/legit_click_aim_assist.cpp",
    "src/modules/combat/aim_assist.hpp", "src/modules/combat/aim_assist.cpp",
    "tests/aimassist_math_test.cpp", "tests/aimassist_config_test.cpp",
]
texts = {source: (ROOT / source).read_text(encoding="utf-8") for source in sources}
for source in sources[:2]:
    includes = re.findall(r'^#include\s+["<]([^">]+)', texts[source], re.M)
    assert not any(any(word in include.lower() for word in ("windows", "jni", "imgui", "cache", "mappings")) for include in includes), source
engine = texts["src/modules/combat/legit_aim_engine.cpp"]
assert "GetAsyncKeyState" not in engine and "::cache" not in engine
assert "physical_button_down" in engine and "capture_client_context" in engine
assert "settings.strength <= 0" in engine and "delta_time > .10f" in engine
assert "geometry::filtered" in engine and "geometry::manual_input" in engine
assert "geometry::motion(state.motion," in engine
assert engine.count("geometry::reset_motion(state.motion)") >= 8
assert "geometry::observe_target(state.motion, observed_target, delta_time)" in engine
assert "geometry::stop_motion(state.motion)" in engine
assert "damped_axis" in texts["src/modules/combat/legit_aim_math.hpp"]
assert "strength = 42.f" in texts["src/modules/combat/legit_aim_config.hpp"]
assert "check_visibility" not in engine
mapping = (ROOT / "src/sdk/minecraft/common/entities.hpp").read_text(encoding="utf-8")
# The 1.21 spellings remain the macro's input; 26.1 selects its own compile-time names.
mapping = re.sub(r'ONESTEP_MC_NAME\(("[^"]+")\)', r'\1', mapping)
assert re.search(r'jni::method<jdouble,\s*"method_23320">\s+getEyeY', mapping)
assert re.search(r'jni::method<jfloat,\s*"method_5751">\s+getStandingEyeHeight', mapping)
assert "context.player.getEyeY.is_valid()" in engine
assert "state.reset_generation.load(std::memory_order_acquire) != generation" in engine
assert "head = 0, upper_body = 1, adaptive = 4" in texts["src/modules/combat/legit_aim_config.hpp"]
for module in ("legit_click_aim_assist", "aim_assist"):
    header = texts[f"src/modules/combat/{module}.hpp"]
    source = texts[f"src/modules/combat/{module}.cpp"]
    assert "render_when_disabled()" in header and "request_reset(state)" in header
    worker = re.search(rf'void modules::{module}::run\([^)]*\)\s*\{{(.*?)\}}', source, re.S).group(1)
    assert "legit_aim_engine::run" not in worker
    assert f"void modules::{module}::render_overlay()" in source
    assert "legit_aim_engine::reset" not in source
catalog = (ROOT / "src/modules/module_catalog.cpp").read_text(encoding="utf-8")
assert '{ "legit_click_aim_assist",' in catalog and '{ "aim_assist",' in catalog
if options.parser_dir:
    sys.path.insert(0, str(options.parser_dir.resolve()))
    from tree_sitter import Language, Parser
    import tree_sitter_cpp
    syntax = Parser(Language(tree_sitter_cpp.language()))
    errors = []
    for source, content in texts.items():
        tree = syntax.parse(content.encode("utf-8"))
        pending = [tree.root_node]
        while pending:
            node = pending.pop()
            if node.type == "ERROR" or node.is_missing:
                errors.append(f"{source}:{node.start_point.row + 1}: {node.type}")
            pending.extend(node.children)
    if errors:
        raise SystemExit("\n".join(errors))
    print(f"C++ syntax parsed for {len(sources)} Aim Assist files.")
print("Aim source boundaries, physical input, client-thread ownership, zero strength and config ids checked. No build or runtime tests executed.")
