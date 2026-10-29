"""Compile and run offline C++20 tests; does not attach to Minecraft or start a JVM."""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import argparse
import json
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
TESTS = {
    "key_codes": ["tests/key_codes_test.cpp"],
    "keybinds_presentation": ["tests/keybinds_presentation_test.cpp"],
    "menu_background": ["tests/menu_background_test.cpp"],
    "dock_layout": ["tests/dock_layout_test.cpp"],
    "menu_input_policy": ["tests/menu_input_policy_test.cpp"],
    "popup_controls": ["tests/popup_controls_test.cpp", "include/imgui/imgui.cpp", "include/imgui/imgui_draw.cpp",
                       "include/imgui/imgui_widgets.cpp", "include/imgui/imgui_tables.cpp"],
    "menu_background_renderer": ["tests/menu_background_renderer_test.cpp", "src/ui/menu_background.cpp", "src/ui/themes_page.cpp",
                                 "include/imgui/imgui.cpp", "include/imgui/imgui_draw.cpp", "include/imgui/imgui_widgets.cpp", "include/imgui/imgui_tables.cpp"],
    "module_runtime": ["tests/module_runtime_test.cpp"],
    "vape_adapter": ["tests/vape_adapter_test.cpp"],
    "aimassist_math": ["tests/aimassist_math_test.cpp"],
    "trigger_input": ["tests/trigger_input_test.cpp", "src/services/game/attack_coordinator.cpp"],
    "input_service": ["tests/input_service_test.cpp"],
    "pot_refill_engine": ["tests/pot_refill_engine_test.cpp"],
    "anti_bot_engine": ["tests/anti_bot_engine_test.cpp"],
    "item_esp_geometry": ["tests/item_esp_geometry_test.cpp"],
    "cosmetic_physics": ["tests/cosmetic_physics_test.cpp"],
    "config_fields": ["tests/config_fields_test.cpp", "src/modules/core/config_fields.cpp"],
    "aimassist_config": ["tests/aimassist_config_test.cpp", "src/modules/core/config_fields.cpp"],
    "jump_reset_config": ["tests/jump_reset_config_test.cpp", "src/modules/core/config_fields.cpp"],
    "trigger_config": ["tests/trigger_config_test.cpp", "src/modules/core/config_fields.cpp", "src/modules/combat/trigger_engine.cpp"],
    "vape_config": ["tests/vape_config_test.cpp", "src/modules/core/config_fields.cpp"],
    "sdk_mappings": ["tests/sdk_mappings_test.cpp"],
    "minecraft_version": ["tests/minecraft_version_test.cpp"],
    "jni_member_cache": ["tests/jni_member_cache_test.cpp"],
    "settings_store": ["tests/settings_store_test.cpp"],
    "utility_settings": ["tests/utility_settings_test.cpp", "src/modules/core/config_fields.cpp"],
    "remaining_settings": ["tests/remaining_settings_test.cpp", "src/modules/core/config_fields.cpp"],
    "settings_renderer": ["tests/settings_renderer_test.cpp"],
    "public_config": ["tests/public_config_test.cpp", "src/modules/core/public_config.cpp", "src/modules/core/config_fields.cpp"],
    "config_service": ["tests/config_service_test.cpp"],
    "snapshot_session": ["tests/snapshot_session_test.cpp"],
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default=os.environ.get("CXX", "cl" if os.name == "nt" else "g++"))
    parser.add_argument("--jobs", type=int, default=2)
    parser.add_argument("--minecraft-version", choices=("1.21.11", "26.1", "26.1.1", "26.1.2"), default="1.21.11")
    parser.add_argument("--test", action="append", choices=TESTS, help="Run only these tests (repeatable).")
    args = parser.parse_args()
    compiler = shutil.which(args.compiler)
    if not compiler:
        parser.error("C++ compiler not found; set --compiler or CXX (MSVC: use Developer PowerShell).")
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    msvc = Path(compiler).name.lower() in ("cl", "cl.exe", "clang-cl", "clang-cl.exe")
    with tempfile.TemporaryDirectory(prefix="onestep-offline-") as directory:
        def run_test(item):
            name, sources = item
            work = Path(directory) / name
            work.mkdir()
            output = work / (name + (".exe" if os.name == "nt" else ""))
            command = [compiler]
            if msvc:
                command += ["/nologo", "/std:c++20", "/EHsc", "/UNDEBUG", "/W4", f"/Fe:{output}"]
            else:
                command += ["-std=c++20", "-UNDEBUG", "-Wall", "-Wextra", "-pedantic", "-pthread", "-o", str(output)]
            target_id = {"1.21.11": 12111,
                         "26.1": 26100, "26.1.1": 26101, "26.1.2": 26102}[args.minecraft_version]
            target_define = f"ONESTEP_MINECRAFT_VERSION={target_id}"
            command += [f"/D{target_define}"] if msvc else [f"-D{target_define}"]
            if name in ("settings_renderer", "menu_background", "menu_background_renderer", "popup_controls"):
                command += [f"/I{ROOT / 'include'}"] if msvc else ["-I", str(ROOT / "include")]
            if name == "menu_background_renderer" and os.name != "nt":
                command += ["-I", str(ROOT / "tests/support")]
            if name == "input_service" and os.name != "nt":
                command += ["-I", str(ROOT / "tests/support")]
            if name in ("sdk_mappings", "minecraft_version"):
                platform = "win32" if os.name == "nt" else "linux"
                includes = [ROOT / "include/jni", ROOT / "include/jni" / platform]
                if name == "sdk_mappings":
                    # The Block ESP header owns ImGui-backed picker/icon state.
                    # Both the executable and isolated header probes need the
                    # same public include root as the application build.
                    includes.append(ROOT / "include")
                for include in includes:
                    command += [f"/I{include}"] if msvc else ["-I", str(include)]
            if name == "sdk_mappings":
                sdk = ROOT / "src/sdk/minecraft/common"
                headers = [sdk / name for name in json.loads((sdk / "manifest.json").read_text(encoding="utf-8"))["headers"]]
                headers += [ROOT / name for name in (
                    "src/services/game/overlay_context.hpp",
                    "src/services/game/loaded_chunks.hpp", "src/modules/visual/item_esp.hpp",
                    "src/services/game/anti_bot_service.hpp", "src/modules/combat/anti_bot.hpp",
                    "src/modules/visual/storage_esp.hpp", "src/modules/visual/block_esp.hpp")]
                # Each domain must compile without a later umbrella completing it.
                for header in headers:
                    probe = work / "header_check.cpp"
                    probe.write_text(f'#include "{header.as_posix()}"\n', encoding="utf-8")
                    if header.name == "overlay_context.hpp":
                        probe.write_text(probe.read_text(encoding="utf-8") +
                            "void check_overlay(services::game::overlay_session& session, "
                            "const maps::ClientWorld& world, const maps::ClientPlayerEntity& player) { "
                            "session.observe({world, player}); session.accepts(session.ticket()); session.reset(); }\n",
                            encoding="utf-8")
                    syntax = [compiler]
                    syntax += ["/nologo", "/std:c++20", "/EHsc", "/Zs"] if msvc else ["-std=c++20", "-fsyntax-only", "-pthread"]
                    syntax += [f"/D{target_define}"] if msvc else [f"-D{target_define}"]
                    for include in includes:
                        syntax += [f"/I{include}"] if msvc else ["-I", str(include)]
                    checked = subprocess.run(syntax + [str(probe)], cwd=work, capture_output=True, text=True)
                    if checked.returncode:
                        return name, False, f"Standalone SDK header failed: {header}\n" + checked.stdout + checked.stderr
            if name == "sdk_mappings" and target_id >= 26100:
                sources = ["tests/minecraft261_sdk_test.cpp"]
            command += [str(ROOT / source) for source in sources]
            built = subprocess.run(command, cwd=work, capture_output=True, text=True)
            if built.returncode:
                return name, False, built.stdout + built.stderr
            ran = subprocess.run([str(output)], cwd=work, capture_output=True, text=True)
            return name, ran.returncode == 0, built.stdout + built.stderr + ran.stdout + ran.stderr

        with ThreadPoolExecutor(max_workers=args.jobs) as pool:
            selected = {name: TESTS[name] for name in args.test} if args.test else TESTS
            results = list(pool.map(run_test, selected.items()))
        for name, passed, detail in results:
            print(f"{'PASS' if passed else 'FAIL'} {name}")
            if detail.strip():
                print(detail.rstrip())
        return 0 if all(passed for _, passed, _ in results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
