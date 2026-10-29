#include "public_config.hpp"
#include "module_registry.hpp"
#include "config_json.hpp"
#include "totem_config_migration.hpp"
#include "../../platform/input/key_codes.hpp"
#include <set>
#include <vector>

std::string modules::export_public_config()
{
    std::string output = "{\"modules\":[";
    bool first = true;
    for (const module* current : get_modules()) {
        if (!current) continue;
        if (!first) output += ',';
        first = false;
        output += "{\"id\":\"" + config_json::escape(current->get_id()) + "\",\"enabled\":" +
            (current->enabled ? "true" : "false") + ",\"keybind\":" + std::to_string(current->get_keybind()) +
            ",\"settings\":" + current->export_config_settings() + "}";
    }
    return output + "]}";
}

bool modules::apply_public_config(const std::string& payload, std::string& message)
{
    using config_json::kind;
    const auto document = config_json::parse(payload);
    if (!document || document->type != kind::object) {
        message = "Invalid config JSON (check syntax, duplicate fields and the 96 KB limit).";
        return false;
    }
    // Read old documents without making a version field part of new configs.
    if (const auto* legacy = document->find("version"); legacy && legacy->integer() != 3) {
        message = "Unsupported legacy config format."; return false;
    }
    const auto* list = document->find("modules");
    if (!list || list->type != kind::array || list->items.size() > 128) {
        message = "Config must contain a modules list with at most 128 entries."; return false;
    }
    struct pending_module {
        module* target;
        bool enabled;
        int keybind;
        std::string settings, previous;
    };
    std::vector<pending_module> pending;
    std::set<std::string> seen;
    const config_json::value* old_totem = nullptr;
    const config_json::value* old_legit_totem = nullptr;
    for (const auto& entry : list->items) {
        if (entry.type != kind::object) { message = "Config module entry must be an object."; return false; }
        const auto* id = entry.find("id");
        const auto* enabled = entry.find("enabled");
        const auto* keybind = entry.find("keybind");
        const auto* settings = entry.find("settings");
        if (!id || id->type != kind::string || id->text.empty() || id->text.size() > 128) {
            message = "Config module ID is missing or invalid."; return false;
        }
        if (!seen.insert(id->text).second) { message = id->text + ": duplicate module ID."; return false; }
        const auto binding = keybind ? keybind->integer() : std::nullopt;
        if (!enabled || enabled->type != kind::boolean || !binding || !platform::keys::valid(*binding) ||
            !settings || settings->type != kind::object) {
            message = id->text + ": expected enabled (bool), a valid keybind and settings (object).";
            return false;
        }
        const bool totem_alias = id->text == "auto_totem" || id->text == "legit_auto_totem";
        module* current = find_module_by_id(totem_alias ? "auto_totem" : id->text);
        if (!current) continue; // Removed/unavailable modules do not block compatible ones.
        if (totem_alias)
        {
            const bool legit = id->text == "legit_auto_totem";
            const auto converted = config_migration::encode(config_migration::totem_fields(*settings, legit));
            if (!current->validate_config_settings(converted)) {
                message = id->text + ": " + config_validation_error(*current, converted); return false;
            }
            (legit ? old_legit_totem : old_totem) = &entry;
            continue;
        }
        const std::string fields(settings->raw);
        if (!current->validate_config_settings(fields)) {
            message = id->text + ": " + config_validation_error(*current, fields); return false;
        }
        pending.push_back({current, enabled->boolean, *binding, fields, current->export_config_settings()});
    }
    if (old_totem || old_legit_totem)
    {
        auto* target = find_module_by_id("auto_totem");
        const auto merged = config_migration::merge_totem(old_totem, old_legit_totem);
        if (!target->validate_config_settings(merged.settings)) {
            message = "auto_totem: " + config_validation_error(*target, merged.settings); return false;
        }
        pending.push_back({target, merged.enabled, merged.keybind, merged.settings, target->export_config_settings()});
    }
    if (pending.empty()) { message = "This config has no compatible modules."; return false; }
    // Every entry is checked before any mutation. Restore earlier settings if an
    // unexpected application failure occurs; enabled/keybinds publish only last.
    for (std::size_t i = 0; i < pending.size(); ++i) {
        if (pending[i].target->apply_config_settings(pending[i].settings)) continue;
        bool restored = true;
        for (std::size_t j = 0; j <= i; ++j)
            if (!pending[j].target->apply_config_settings(pending[j].previous)) restored = false;
        message = std::string(pending[i].target->get_id()) +
            (restored ? ": could not apply config; previous settings restored." :
                ": could not apply config; some previous settings could not be restored.");
        return false;
    }
    for (const auto& item : pending) {
        item.target->enabled = item.enabled;
        item.target->set_keybind(item.keybind);
    }
    message = std::to_string(pending.size()) + " module settings loaded.";
    return true;
}

std::string modules::export_keybind_config()
{
    std::string output = "{\"modules\":[";
    bool first = true;
    for (const auto* entry : get_modules()) {
        if (!entry || !entry->supports_keybind()) continue;
        if (!first) output += ',';
        first = false;
        output += "{\"id\":\"" + config_json::escape(entry->get_id()) + "\",\"keybind\":" +
            std::to_string(entry->get_keybind()) + "}";
    }
    return output + "]}";
}

bool modules::apply_keybind_config(const std::string& payload, std::string& message)
{
    using config_json::kind;
    const auto document = config_json::parse(payload);
    const auto* list = document && document->type == kind::object ? document->find("modules") : nullptr;
    if (!list || list->type != kind::array || list->items.size() > 128) {
        message = "Invalid local keybind profile."; return false;
    }
    std::set<std::string> seen;
    std::vector<std::pair<module*, int>> pending;
    for (const auto& entry : list->items) {
        const auto* id = entry.type == kind::object ? entry.find("id") : nullptr;
        const auto* binding = entry.find("keybind");
        const auto value = binding ? binding->integer() : std::nullopt;
        if (!id || id->type != kind::string || id->text.empty() || id->text.size() > 128 ||
            !seen.insert(id->text).second || !value || !platform::keys::valid(*value)) {
            message = "Invalid or duplicate keybind entry; no changes applied."; return false;
        }
        if (auto* target = find_module_by_id(id->text); target && target->supports_keybind())
            pending.emplace_back(target, *value);
    }
    for (const auto& [target, value] : pending) target->set_keybind(value);
    message = "Local keybinds loaded.";
    return true;
}
