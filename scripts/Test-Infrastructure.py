"""Validate the active source/build boundaries without Windows or third-party Python packages."""
from pathlib import Path
import json
import re
import xml.etree.ElementTree as ET
from repository_hygiene import validate_repository

ROOT = Path(__file__).resolve().parent.parent
KINDS = {"ClCompile", "ClInclude", "ResourceCompile", "None"}


def validate_sdk():
    errors = []
    sdk = ROOT / "src/sdk/minecraft/common"
    manifest = json.loads((sdk / "manifest.json").read_text(encoding="utf-8"))
    if manifest.get("baseline_version") != "1.21.11" or manifest.get("loader") != "Fabric" or manifest.get("namespace") != "maps":
        errors.append("Unexpected SDK target in mapping manifest")
    if manifest.get("targets") != ["1.21.11", "26.1", "26.1.1", "26.1.2"]:
        errors.append("SDK manifest must list each explicit Minecraft build target")
    if set(manifest["headers"]) != {p.name for p in sdk.glob("*.hpp")}:
        errors.append("SDK header list and manifest differ")
    classes = {}
    for filename in manifest["headers"]:
        source = (sdk / filename).read_text(encoding="utf-8")
        if '#include "../../../mappings.hpp"' in source:
            errors.append(f"SDK domain depends on compatibility umbrella: {filename}")
        for name, java_name in re.findall(r'(?:BEGIN_KLASS_DEF(?:_EX)?|KLASS_DECLARATION)\(\s*([\w$]+),\s*(?:ONESTEP_MC_NAME\()?"([^"]+)"', source):
            # Target-only helper is not part of the existing 1.21.x class manifest.
            if name in manifest.get("staged_classes", {}).get("26.1", {}):
                if java_name != manifest["staged_classes"]["26.1"][name]:
                    errors.append(f"Staged 26.1 class differs from manifest: {name}")
                continue
            if name in classes:
                errors.append(f"Duplicate SDK mapping: {name}")
            classes[name] = java_name
        for include in re.findall(r'^\s*#include\s*"([^"]+)"', source, re.M):
            if not (sdk / include).is_file():
                errors.append(f"Missing SDK include: {filename}: {include}")
    if classes != manifest["classes"]:
        errors.append("SDK class names differ from the version manifest")
    if len((ROOT / "src/mappings.hpp").read_text(encoding="utf-8").splitlines()) > 10:
        errors.append("mappings.hpp must remain a compatibility umbrella")
    return errors



def validate_settings():
    """Every catalog entry must declare its config and panel ownership explicitly."""
    errors = []
    path = ROOT / "src/modules/settings_manifest.json"
    manifest = json.loads(path.read_text(encoding="utf-8"))
    entries = manifest["modules"]
    catalog = (ROOT / "src/bridge/ModuleCatalog.java").read_text(encoding="utf-8")
    ids = re.findall(r'^\s*m\("([a-z0-9_]+)"', catalog, re.M)
    covered = [entry["id"] for entry in entries]
    if manifest.get("version") != 2 or manifest.get("authority") != "src/bridge/ModuleCatalog.java" or len(covered) != len(set(covered)) or set(covered) != set(ids):
        errors.append("Settings manifest must cover each catalog ID exactly once")
    no_config = {"friends_manager", "click_gui", "panic", "full_bright", "no_jump_delay"}
    if {e["id"] for e in entries if e["model"] is None} != no_config:
        errors.append("Unexpected modules without settings models")
    allowed = {"no_module_settings", "java_manager_view"}
    inspected = set()
    for entry in entries:
        if entry["ownership"] not in allowed:
            errors.append(f"Unknown config ownership: {entry['id']}")
        owner = ROOT / entry["owner"]
        if not owner.is_file():
            errors.append(f"Missing config owner: {entry['id']}")
            continue
        header = owner.read_text(encoding="utf-8")
        if entry["model"] is None:
            if "visit_config(" in header:
                errors.append(f"Untracked config on no-settings module: {entry['id']}")
            continue
        model, panel = ROOT / entry["model"], ROOT / entry["panel"]
        if not model.is_file() or not panel.is_file():
            errors.append(f"Missing config model/panel: {entry['id']}")
            continue
        pure = model.read_text(encoding="utf-8")
        ui = panel.read_text(encoding="utf-8")
        if "schema(" not in pure or "modules::settings::descriptor<" not in pure:
            errors.append(f"Module model bypasses schema: {entry['id']}")
        if not re.search(r"gui::settings::(?:render|edit)\(", ui):
            errors.append(f"Panel bypasses shared settings: {entry['id']}")
        if '#include "../../ui/settings_renderer.hpp"' not in ui:
            errors.append(f"Panel must explicitly include the settings renderer: {entry['id']}")
        if re.search(r"\b(?:v|visitor)\.(?:field|enum_field|bounded_enum)\(", header):
            errors.append(f"Raw config fields remain in module owner: {entry['id']}")
        if entry["ownership"] == "snapshot_store":
            if "settings_.visit(" not in header or "settings_.read()" not in (ui + header):
                errors.append(f"Module must use shared store and snapshots: {entry['id']}")
        for source in (owner, model, panel):
            if source in inspected:
                continue
            inspected.add(source)
            for include in re.findall(r'^\s*#include\s*"([^"]+)"', source.read_text(encoding="utf-8"), re.M):
                if not (source.parent / include).is_file():
                    errors.append(f"Broken settings include: {source.relative_to(ROOT)}: {include}")
        if any(term in pure for term in ("ImGui::", "gui::", "<Windows.h>", "<jni.h>", "mappings.hpp")):
            errors.append(f"Settings model depends on UI/game/platform: {entry['id']}")
    listed = {ROOT / e["model"] for e in entries if e["model"]}
    for folder in ("misc", "combat", "visual", "render", "client"):
        for model in (ROOT / f"src/modules/{folder}/settings").glob("*_settings.hpp"):
            if model not in listed:
                errors.append(f"Untracked settings model: {model.relative_to(ROOT)}")
    return errors


def validate_overlay_sessions():
    errors = []
    for name in ("block_esp", "storage_esp", "radar"):
        source = (ROOT / f"src/modules/visual/{name}.cpp").read_text(encoding="utf-8")
        panel = source.split(f"void modules::{name}::render_overlay()", 1)[1].split("void modules::", 1)[0]
        if "capture_client_context(context, true, false)" not in panel:
            errors.append(f"Overlay must check the live world every frame: {name}")
        if not re.search(r"session_?\.observe\(\{context.world, context.player\}\)", panel):
            errors.append(f"Overlay snapshot must belong to the current world/player: {name}")
        if not re.search(r"if \(!session_?\.accepts\(generation\)\) return;", source):
            errors.append(f"Overlay worker must reject previous-session results: {name}")
        if "clear_snapshot(); return;" not in panel and "clear_tag_snapshot(); return;" not in panel:
            errors.append(f"Disconnected overlay must clear its snapshot: {name}")
    player = (ROOT / "src/modules/visual/player_esp.cpp").read_text(encoding="utf-8")
    panel = player.split("void modules::player_esp::render_overlay()", 1)[1].split("void modules::", 1)[0]
    if "boxes = s_player_boxes" in panel or "capture_client_context(context, true, false)" not in panel:
        errors.append("Player ESP must never fall back to boxes from a disconnected world")
    if "!read_render_camera(context," not in panel or "!read_render_players(context, boxes)" not in panel:
        errors.append("Player ESP must require a live camera and player list from the same context")
    return errors

def validate_esp_ranges():
    errors = []
    for name in ("storage_esp", "block_esp", "chams", "item_esp"):
        model = (ROOT / f"src/modules/visual/settings/{name}_settings.hpp").read_text(encoding="utf-8")
        if re.search(r'"(?:range|max_distance)"', model):
            errors.append(f"ESP must not expose a draw-distance setting: {name}")
    for name in ("storage_esp", "block_esp"):
        source = (ROOT / f"src/modules/visual/{name}.cpp").read_text(encoding="utf-8")
        if "capture_loaded_chunks(cache.world, chunks)" not in source or "cache.world.getBlockState" in source:
            errors.append(f"ESP must scan loaded chunks, not a radius around the player: {name}")
    item = (ROOT / "src/modules/visual/item_esp.cpp").read_text(encoding="utf-8")
    if "capture_client_context(context, true, false)" not in item or "get_entities(context.world)" not in item:
        errors.append("Item ESP must read the live world's entities every frame")
    return errors


def validate_anti_bot():
    errors = []
    owner = (ROOT / "src/modules/combat/anti_bot.cpp").read_text(encoding="utf-8")
    model = (ROOT / "src/modules/combat/settings/anti_bot_settings.hpp").read_text(encoding="utf-8")
    catalog = (ROOT / "src/bridge/ModuleCatalog.java").read_text(encoding="utf-8")
    if not re.search(r'm\("anti_bot",\s*"anti bot",\s*"Client",\s*true,', catalog) or "enabled =" in owner:
        errors.append("AntiBot must default on without overriding imported module state")
    consumers = {
        "modules/combat/legit_aim_engine.cpp": 1, "modules/combat/click_aim_engine.cpp": 1,
        "services/game/world_bridge.cpp": 1, "modules/vape/vape_module.cpp": 1,
        "modules/visual/player_esp.cpp": 2,
        "modules/visual/chams.cpp": 1,
        "modules/visual/radar.cpp": 1, "services/game/target_service.cpp": 1,
        "services/game/attack_service.cpp": 1,
    }
    for filename, minimum in consumers.items():
        text = (ROOT / "src" / filename).read_text(encoding="utf-8")
        if text.count("services::anti_bot::is_bot(") < minimum:
            errors.append(f"Missing shared AntiBot target filter: {filename}")
    service = (ROOT / "src/services/game/anti_bot_service.cpp").read_text(encoding="utf-8")
    for token in ("getPlayerListEntry.call(uuid)", "getPlayerListEntry.call(local_uuid)",
                  "entity_world.is_same_object(context.world)", "session.observe({context.world, context.player})"):
        if token not in service: errors.append(f"AntiBot must use reliable UUID/session data: {token}")
    if "sendPacket" in service or "setGlowing" in service:
        errors.append("AntiBot classification must remain read-only")
    return errors


def validate_pot_refill():
    errors = []
    source = (ROOT / "src/modules/misc/pot_refill.cpp").read_text(encoding="utf-8")
    header = (ROOT / "src/modules/misc/pot_refill.hpp").read_text(encoding="utf-8")
    run = source.split("void modules::pot_refill::run(::cache&)", 1)[1].split(
        "void modules::pot_refill::render_overlay()", 1)[0]
    if "clickSlot.call" in run or source.count("clickSlot.call") != 1:
        errors.append("PotRefill must submit at most one inventory action on the client/render thread")
    for guard in ("capture_client_context(context)", "state.session.matches({context.world, player})",
                  "state.screen.is_same_object(context.screen)", "state.handler.is_same_object(handler)",
                  "state.sync_id == sync_id", "!enabled", "GetForegroundWindow()", "gui::draw",
                  "CreativeInventoryScreen", "physical_button_down", "getCursorStack.call()",
                  "target_stack.isEmpty.call()", "canInsert.call(source_stack)",
                  "areItemsAndComponentsEqual.call", "state.engine.observe(now, observed)"):
        if guard not in source:
            errors.append(f"Missing PotRefill session/action guard: {guard}")
    if "render_when_disabled() const override { return true; }" not in header:
        errors.append("PotRefill must release pending state on the render thread when disabled")
    return errors


def project_items(path):
    result = []
    for node in ET.parse(path).iter():
        kind = node.tag.rsplit("}", 1)[-1]
        include = node.get("Include")
        if kind in KINDS and include:
            result.append((kind, include))
    return result


def validate_project_entries(project, filters):
    errors = []
    if set(project) != set(filters):
        errors.append("Project/filter entries differ; run scripts/Refresh-ProjectFilters.py")
    if len(project) != len(set(project)) or len(filters) != len(set(filters)):
        errors.append("Duplicate project/filter entries")
    return errors


def validate():
    errors = (validate_repository(ROOT) + validate_sdk() + validate_settings() + validate_overlay_sessions() + validate_esp_ranges()
              + validate_anti_bot() + validate_pot_refill()
             )
    project = project_items(ROOT / "OneStep.vcxproj")
    filters = project_items(ROOT / "OneStep.vcxproj.filters")
    errors.extend(validate_project_entries(project, filters))
    compiled = set()
    for kind, include in sorted(project):
        normalized = include.replace("\\", "/")
        matches = list(ROOT.glob(normalized)) if any(c in normalized for c in "*?") else [ROOT / normalized]
        if not matches or any(not path.is_file() for path in matches):
            errors.append(f"Missing project input: {include}")
        if kind == "ClCompile":
            for path in matches:
                path = path.resolve()
                if path in compiled:
                    errors.append(f"Source compiled twice: {include}")
                compiled.add(path)

    active_sources = set((ROOT / "src").rglob("*.cpp"))
    for path in sorted(active_sources - compiled):
        errors.append(f"Source missing from build: {path.relative_to(ROOT)}")

    catalog = (ROOT / "src/bridge/ModuleCatalog.java").read_text(encoding="utf-8")
    ids = re.findall(r'^\s*m\("([a-z0-9_]+)"', catalog, re.M)
    if not ids or len(ids) != len(set(ids)):
        errors.append("Module catalog IDs must be nonempty and unique")
    vape = {entry for entry in ids if entry.startswith("vape_")}
    if vape != {"vape_aim_assist"}:
        errors.append("Vape catalog must contain only Aim Assist")
    if len((ROOT / "src/modules/modules.hpp").read_text(encoding="utf-8").splitlines()) > 80:
        errors.append("modules.hpp must remain a small compatibility umbrella")

    for path in (ROOT / "src").rglob("*.cpp"):
        content = path.read_text(encoding="utf-8")
        if re.search(r'bool\s+(clear_java_exception|ensure_client_jni_env)\s*\(', content):
            errors.append(f"Duplicated JNI helper: {path.relative_to(ROOT)}")
        if (ROOT / "src/modules") in path.parents:
            if re.search(r'\bSendInput\s*\(', content):
                errors.append(f"Direct module SendInput: {path.relative_to(ROOT)}")
            if re.search(r'selectedSlot\.set\s*\(', content):
                errors.append(f"Direct module hotbar write: {path.relative_to(ROOT)}")

    main = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
    if len(main.splitlines()) > 60 or any(term in main for term in ("mappings.hpp", "cache.hpp", "auth.hpp")):
        errors.append("DLL entry must delegate application logic to src/app")
    pch = (ROOT / "include/pch.hpp").read_text(encoding="utf-8")
    if re.search(r'^\s*#\s*include\s*"', pch, re.M):
        errors.append("PCH must not include project headers")
    setting_models = [ROOT / "src/modules/core/setting_schema.hpp", ROOT / "src/modules/core/settings_store.hpp",
                      ROOT / "src/modules/vape/vape_config.hpp", *(ROOT / "src/modules/misc/settings").glob("*_settings.hpp")]
    for path in setting_models:
        content = path.read_text(encoding="utf-8")
        if any(term in content for term in ("ImGui::", "gui::", "<Windows.h>", "<jni.h>", "mappings.hpp")):
            errors.append(f"Settings model depends on UI/game/platform: {path.relative_to(ROOT)}")
    for model in (ROOT / "src/modules/misc/settings").glob("*_settings.hpp"):
        name = model.stem.removesuffix("_settings")
        header = ROOT / f"src/modules/misc/{name}.hpp"
        source = ROOT / f"src/modules/misc/{name}.cpp"
        for path in (model, header, source):
            for include in re.findall(r'^\s*#include\s*"([^"]+)"', path.read_text(encoding="utf-8"), re.M):
                if not (path.parent / include).is_file():
                    errors.append(f"Broken migrated module include: {path.relative_to(ROOT)}: {include}")
        if 'settings_' not in header.read_text(encoding="utf-8") or 'visit_config(' in header.read_text(encoding="utf-8"):
            errors.append(f"Module must expose a Java-owned settings view only: {name}")
        content = source.read_text(encoding="utf-8")
        if (name not in ("fast_exp", "fast_place", "sprint") and "settings_.read()" not in content) or "gui::settings::render(settings_," not in content:
            errors.append(f"Module must use settings snapshots and common UI: {name}")
    for path in [*(ROOT / "src/app").rglob("*.cpp"), *(ROOT / "src/app").rglob("*.hpp"), ROOT / "src/platform/jni/jvm_session.cpp"]:
        for include in re.findall(r'^\s*#include\s*"([^"]+)"', path.read_text(encoding="utf-8"), re.M):
            if not (path.parent / include).is_file():
                errors.append(f"Broken application include: {path.relative_to(ROOT)}: {include}")
    return errors, len(ids), len(compiled)


if __name__ == "__main__":
    errors, modules, sources = validate()
    if errors:
        raise SystemExit("\n".join(errors))
    print(f"Infrastructure passed: {modules} module IDs, {sources} compile inputs, matching filters, SDK manifest, settings/service boundaries and repository hygiene.")
