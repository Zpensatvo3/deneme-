"""Source-only Jump Reset checks: no builds, executable tests or input events."""
from pathlib import Path
import argparse
import re
import sys

ROOT = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--parser-dir", type=Path)
options = parser.parse_args()
sources = ["src/modules/combat/jump_reset_config.hpp", "src/modules/combat/jump_reset.hpp",
           "src/modules/combat/jump_reset.cpp", "tests/jump_reset_config_test.cpp",
           "src/services/input/input_service.hpp",
           "src/services/input/input_service.cpp", "src/gui/gui.cpp", "src/main.cpp",
           "src/services/game/world_bridge.hpp"]
texts = {source: (ROOT / source).read_text(encoding="utf-8") for source in sources}
config, header, implementation = (texts[source] for source in sources[:3])
includes = re.findall(r'^#include\s+["<]([^">]+)', config, re.M)
assert not any(any(word in include.lower() for word in ("windows", "jni", "imgui", "cache", "mappings")) for include in includes)
fields = re.findall(r'd::(?:choice|integer|toggle)\("([^"]+)"', config)
assert fields == ["mode", "chance", "jump_delay_ms", "only_combat"]
assert "modules::settings::visit_fields(visitor, candidate, schema())" in config
assert "settings_snapshot()" in header and "apply_settings(candidate, true)" in header
assert "Success Chance" not in implementation and "Real Space Press" not in implementation
assert 'width >= 280.f' in implementation and 'profile_names, 4' in implementation
assert "ImGui::CalcTextSize(profile_names[index]).x + 16.f <= button_width" in implementation
panel = implementation[implementation.index("void modules::jump_reset::render_options()") :]
assert "help(" not in panel and "PushTextWrapPos" not in panel
assert "return 320.0f" in header
assert 'gui::option_section("Timing")' in implementation
assert 'gui::option_section("Input / conditions")' in implementation
assert "GetAsyncKeyState" not in implementation
worker = re.search(r'void modules::jump_reset::run\([^)]*\)\s*\{(.*?)\}', implementation, re.S).group(1)
assert "setPressed" not in worker and "engine" not in worker
assert "render_when_disabled()" in header and "reset_generation" in header
bridge = (ROOT / "src/bridge/WorldBridge.java").read_text(encoding="utf-8")
native = (ROOT / "src/services/game/world_bridge.cpp").read_text(encoding="utf-8")
assert "jump_reset_engine" not in header + implementation
assert not (ROOT / "src/modules/combat/jump_reset_engine.hpp").exists()
assert "capture_client_context" not in implementation and "setPressed" not in implementation
assert "world_bridge::tick_jump(settings, enabled, reset_generation)" in implementation
assert "std::lock_guard lock(settings_mutex)" in implementation
assert "release_jump_input" not in implementation
assert "on_render_shutdown() override" in header
assert "jumpConsumedSource" in bridge and "jumpSourceAt" in bridge
assert all(token in bridge for token in ("now + 200", "now + 140", "now + 105", "now + 170"))
assert "age > jumpSentAge" in bridge and "jumpCooldown.getInt(p) != 0" in bridge
assert "jumpAllowFlight.getBoolean(abilities)" in bridge
assert '"tickJump", "(ZJIIIZ)V"' in native
assert '"jumpPermitted"), const_cast<char*>("(J)Z")' in native
assert all(token not in native for token in ("press_jump_space", "release_jump_space", "jump_space_receipt"))
assert all(token in bridge for token in ("glfwGetKey", "glfwGetMouseButton", "glfwGetKeyScancode", "glfwGetWindowAttrib"))
assert "jumpSetPressed.invoke(key, true)" in bridge
assert "jumpService" not in bridge and "jumpSystemOwned" not in bridge
assert "jump_reset_source->load(std::memory_order_acquire) == jump_expected_generation" in native
catalog = (ROOT / "src/modules/module_catalog.cpp").read_text(encoding="utf-8")
assert '{ "jump_reset",' in catalog
if options.parser_dir:
    sys.path.insert(0, str(options.parser_dir.resolve()))
    from tree_sitter import Language, Parser
    import tree_sitter_cpp
    syntax = Parser(Language(tree_sitter_cpp.language()))
    errors = []
    for source, content in texts.items():
        content = re.sub(r'\b(?:WINAPI|CALLBACK|APIENTRY|JNICALL)\b', lambda match: ' ' * len(match[0]), content)
        pending = [syntax.parse(content.encode("utf-8")).root_node]
        while pending:
            node = pending.pop()
            if node.type == "ERROR" or node.is_missing:
                errors.append(f"{source}:{node.start_point.row + 1}: {node.type}")
            pending.extend(node.children)
    if errors:
        raise SystemExit("\n".join(errors))
    print(f"C++ syntax parsed for {len(sources)} Jump Reset files.")
print("Jump config, Java runtime delegation, JNI signatures, Java/GLFW game input and dispatch cancellation checked. No build or runtime tests executed.")
