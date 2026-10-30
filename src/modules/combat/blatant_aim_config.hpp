#pragma once
#include "../core/setting_schema.hpp"

namespace blatant_aim_engine
{
    enum class aim_point { body = 1, random = 3, adaptive = 4 };
    struct config
    {
        float fov = modules::catalog::blatant_click_aim_assist_fov_default, max_distance = modules::catalog::blatant_click_aim_assist_max_distance_default, strength = modules::catalog::blatant_click_aim_assist_strength_default;
        aim_point point = static_cast<aim_point>(modules::catalog::blatant_click_aim_assist_aim_point_default);
        bool only_weapon = modules::catalog::blatant_click_aim_assist_only_weapon_default, last_target = modules::catalog::blatant_click_aim_assist_last_target_default;
        bool operator==(const config&) const = default;
    };
    inline std::span<const modules::settings::descriptor<config>> schema()
    {
        using d = modules::settings::descriptor<config>;
        using label = modules::settings::label;
        static constexpr label points[]{{"Body"},{"Random"},{"Adaptive"}};
        static constexpr int values[]{1,3,4};
        static const d fields[]{
            d::number("fov", {"FOV"}, &config::fov, modules::catalog::blatant_click_aim_assist_fov_minimum, modules::catalog::blatant_click_aim_assist_fov_maximum, "%.0f deg"),
            d::number("max_distance", {"Max Distance"}, &config::max_distance, modules::catalog::blatant_click_aim_assist_max_distance_minimum, modules::catalog::blatant_click_aim_assist_max_distance_maximum, "%.1f blocks"),
            d::number("strength", {"Strength"}, &config::strength, modules::catalog::blatant_click_aim_assist_strength_minimum, modules::catalog::blatant_click_aim_assist_strength_maximum, "%.2f"),
            d::toggle("only_weapon", {"Only Weapon"}, &config::only_weapon),
            d::toggle("last_target", {"Last Target"}, &config::last_target),
            d::enumeration<&config::point>("aim_point", {"Aim Point"}, points, values)
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
        // Legacy Head/Legs values map to the existing Body/Adaptive behavior.
        if (static_cast<int>(candidate.point) == 0) candidate.point = aim_point::body;
        if (static_cast<int>(candidate.point) == 2) candidate.point = aim_point::adaptive;
        if (!modules::settings::normalize(candidate, schema())) visitor.reject("Invalid aim settings.");
        visitor.stage_values(false);
        if (visitor.reading() && visitor.valid()) { live = candidate; return true; }
        return false;
    }
}
