#pragma once
#include "config_migration_fields.hpp"

namespace modules::config_migration
{
    inline bool unified_totem(const config_json::value& settings)
    {
        for (const auto& [key, value] : settings.members)
        {
            (void)value;
            if (key == "mode" || key.starts_with("legit_")) return true;
        }
        return false;
    }

    inline fields totem_fields(const config_json::value& settings, bool legit)
    {
        fields result;
        for (const auto& [key, value] : settings.members)
            result[legit ? "legit_" + key : key] = std::string(value.raw);
        return result;
    }

    struct totem_entry { bool enabled; int keybind; std::string settings; };

    // Modern auto_totem entries take precedence. For old separate modules,
    // retain both profiles and choose the enabled one (Normal wins ties).
    inline totem_entry merge_totem(const config_json::value* normal, const config_json::value* legit)
    {
        fields merged;
        if (legit) merged = totem_fields(*legit->find("settings"), true);
        const bool modern = normal && unified_totem(*normal->find("settings"));
        if (normal)
            for (const auto& [key, value] : totem_fields(*normal->find("settings"), false)) merged[key] = value;

        const auto* selected = normal;
        if (!modern && (!normal || (!normal->find("enabled")->boolean && legit && legit->find("enabled")->boolean)))
            selected = legit;
        if (!selected) selected = legit;
        if (!modern) merged["mode"] = selected == legit ? "1" : "0";
        return {selected->find("enabled")->boolean, *selected->find("keybind")->integer(), encode(merged)};
    }
}
