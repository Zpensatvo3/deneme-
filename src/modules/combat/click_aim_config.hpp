#pragma once
#include "../core/setting_schema.hpp"
#include "aim_regions.hpp"

namespace click_aim_engine
{
    struct config
    {
        float fov = modules::catalog::click_aim_assist_fov_default, strength = modules::catalog::click_aim_assist_strength_default;
        std::vector<int> regions = modules::catalog::click_aim_assist_aim_regions_default;
        bool only_weapon = modules::catalog::click_aim_assist_only_weapon_default, last_target = modules::catalog::click_aim_assist_last_target_default;
        bool operator==(const config&) const = default;
    };
    inline std::span<const modules::settings::descriptor<config>> schema()
    {
        using d = modules::settings::descriptor<config>;
        static const d fields[]{
            d::number("fov", {"Minecraft FOV"}, &config::fov, modules::catalog::click_aim_assist_fov_minimum, modules::catalog::click_aim_assist_fov_maximum, "%.0f deg"),
            d::number("strength", {"Strength"}, &config::strength, modules::catalog::click_aim_assist_strength_minimum, modules::catalog::click_aim_assist_strength_maximum, "%.2f"),
            d::toggle("only_weapon", {"Only Weapon"}, &config::only_weapon),
            d::toggle("last_target", {"Last Target"}, &config::last_target),
            d::integer_list("aim_regions", {"Aim Regions"}, &config::regions, 0, aim_regions::count - 1, aim_regions::maximum_selected)
        };
        return fields;
    }
    inline bool visit_config(modules::config_visitor& visitor, config& live)
    {
        config candidate = live;
        const bool importing = visitor.importing();
        if (importing) visitor.stage_values(true);
        modules::settings::visit_fields(visitor, candidate, schema());
        if (!importing) return false;
        if (!modules::settings::normalize(candidate, schema())) visitor.reject("Invalid aim settings.");
        visitor.stage_values(false);
        if (visitor.reading() && visitor.valid()) { live = candidate; return true; }
        return false;
    }
}
