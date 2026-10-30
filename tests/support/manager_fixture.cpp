// Inert UI-service fixture. State-machine/config correctness is covered against
// the real Java core by Test-ModuleManagers.py, not by this test double.
#include "../../src/modules/core/manager.hpp"
#include "../../src/modules/core/config_json.hpp"
#include "../../src/platform/input/key_codes.hpp"
#include <set>

namespace modules::manager
{
    view_scope::view_scope() = default;
    view_scope::~view_scope() = default;
    namespace {
        std::shared_ptr<snapshot> current = std::make_shared<snapshot>();
        result ok() { return {true, "fixture", {}, current->revision}; }
        void mutate() { current = std::make_shared<snapshot>(*current); ++current->revision; }
    }
    std::shared_ptr<const snapshot> read() {
        if (!current->revision) current->revision = 1;
        for (const auto* id : {"watermark", "arraylist", "spotify_hud", "fast_exp"})
            current->modules.emplace(id, module_state{"", "", "{}"});
        // Fixed inert UI fixtures; real catalog/default behavior is tested in Java/JNI.
        if (auto found = current->modules.find("watermark"); found != current->modules.end() && (found->second.settings.empty() || found->second.settings == "{}"))
            found->second.settings = R"fixture({"scale":1,"position_x":10,"position_y":10})fixture";
        if (auto found = current->modules.find("arraylist"); found != current->modules.end() && (found->second.settings.empty() || found->second.settings == "{}"))
            found->second.settings = R"fixture({"scale":1.12,"position_x":10,"position_y":10,"bg_opacity":0.720000029,"bar_width":2,"bar_glow":true,"text_shadow":true,"show_keybinds":true})fixture";
        if (auto found = current->modules.find("spotify_hud"); found != current->modules.end() && (found->second.settings.empty() || found->second.settings == "{}"))
            found->second.settings = R"fixture({"position_x":22,"position_y":60,"background_opacity":0.720000029,"show_when_idle":true,"show_artwork":true,"show_progress":true,"show_visualizer":true,"show_brand":true,"show_lyrics":true,"enable_lyrics":true,"scale":1.20000005,"lyric_lines":1})fixture";
        if (auto found = current->modules.find("fast_exp"); found != current->modules.end() && (found->second.settings.empty() || found->second.settings == "{}"))
            found->second.settings = R"fixture({"min_cps":16,"max_cps":20,"cps":0})fixture";
        return current;
    }
    module_state state(std::string_view id) {
        const auto found = current->modules.find(std::string(id));
        return found == current->modules.end() ? module_state{"", "", "{}"} : found->second;
    }
    std::shared_ptr<const module_state> find(std::string_view id) {
        if (!current->revision) current->revision = 1;
        auto found = current->modules.find(std::string(id));
        if (found == current->modules.end()) found = current->modules.emplace(std::string(id), module_state{"", "", "{}"}).first;
        return std::shared_ptr<const module_state>(current, &found->second);
    }
    bool refresh() { return true; }
    result enable(std::string_view id, bool value) { mutate(); current->modules[std::string(id)].enabled = value; return ok(); }
    result keybind(std::string_view id, int value) {
        if (!platform::keys::valid(value)) return {};
        mutate(); current->modules[std::string(id)].keybind = value; ++current->keybind_revision; return ok();
    }
    result settings(std::string_view id, const std::string& fields, bool validate, std::uint64_t expected) {
        if (expected && expected != current->revision) return {};
        if (!config_json::parse(fields)) return {};
        if (!validate) { mutate(); current->modules[std::string(id)].settings = fields; }
        return ok();
    }
    result export_config(bool keybinds) {
        std::string output = "{\"modules\":["; bool first = true;
        for (const auto& [id, value] : current->modules) {
            if (!first) output += ',';
            first = false;
            output += "{\"id\":\"" + config_json::escape(id) + "\",\"keybind\":" + std::to_string(value.keybind);
            if (!keybinds) output += ",\"enabled\":" + std::string(value.enabled ? "true" : "false") + ",\"settings\":" + (value.settings.empty() ? "{}" : value.settings);
            output += '}';
        }
        auto result = ok(); result.payload = output + "]}"; return result;
    }
    result import_config(const std::string& json, bool keybinds) {
        const auto root = config_json::parse(json); const auto* entries = root ? root->find("modules") : nullptr;
        if (!entries || entries->type != config_json::kind::array) return {};
        std::set<std::string> seen; auto candidate = std::make_shared<snapshot>(*current);
        for (const auto& entry : entries->items) {
            const auto* id = entry.find("id"); const auto* binding = entry.find("keybind");
            if (!id || id->type != config_json::kind::string || !seen.insert(id->text).second ||
                !binding || !binding->integer() || !platform::keys::valid(*binding->integer())) return {};
            auto& state = candidate->modules[id->text]; state.keybind = *binding->integer();
            if (!keybinds) {
                const auto* enabled = entry.find("enabled"); const auto* fields = entry.find("settings");
                if (!enabled || enabled->type != config_json::kind::boolean || !fields) return {};
                state.enabled = enabled->boolean; state.settings = std::string(fields->raw);
            }
        }
        current = candidate; ++current->revision; ++current->keybind_revision; return ok();
    }
    result disable_all() { mutate(); for (auto& [id, value] : current->modules) value.enabled = false; return ok(); }
}
