#pragma once
#include "../core/setting_schema.hpp"
#include <algorithm>
#include <cmath>

namespace legit_aim_engine
{
    // Serialized values are intentionally sparse and remain unchanged.
    enum class aim_point { head = 0, upper_body = 1, adaptive = 4 };
    struct config
    {
        float strength = modules::catalog::aim_assist_strength_default, fov = modules::catalog::aim_assist_fov_default, max_distance = modules::catalog::aim_assist_max_distance_default;
        aim_point point = static_cast<aim_point>(modules::catalog::aim_assist_aim_point_default);
        bool weapon_only = modules::catalog::aim_assist_weapon_only_default;
    };
    inline std::span<const modules::settings::descriptor<config>> schema()
    {
        using d = modules::settings::descriptor<config>;
        using label = modules::settings::label;
        static constexpr label points[]{{"Adaptive"},{"Upper Body"},{"Head"}};
        static constexpr int values[]{4,1,0};
        static const d fields[]{
            d::number("strength", {"Assist Strength"}, &config::strength, modules::catalog::aim_assist_strength_minimum, modules::catalog::aim_assist_strength_maximum, "%.0f%%"),
            d::number("fov", {"FOV"}, &config::fov, modules::catalog::aim_assist_fov_minimum, modules::catalog::aim_assist_fov_maximum, "%.0f deg"),
            d::number("max_distance", {"Range"}, &config::max_distance, modules::catalog::aim_assist_max_distance_minimum, modules::catalog::aim_assist_max_distance_maximum, "%.1f blocks"),
            d::enumeration<&config::point>("aim_point", {"Aim Point"}, points, values),
            d::toggle("weapon_only", {"Weapon Only"}, &config::weapon_only)
        };
        return fields;
    }
    inline bool normalize(config& value)
    {
        if (!std::isfinite(value.strength) || !std::isfinite(value.fov) ||
            !std::isfinite(value.max_distance) || (value.point != aim_point::head &&
                value.point != aim_point::upper_body && value.point != aim_point::adaptive)) return false;
        value.strength = std::clamp(value.strength, 0.f, 100.f);
        value.fov = std::clamp(value.fov, 20.f, 120.f);
        value.max_distance = std::clamp(value.max_distance, 2.5f, 5.f);
        return true;
    }
    inline bool same_config(const config& a, const config& b)
    {
        return a.strength == b.strength && a.fov == b.fov && a.max_distance == b.max_distance &&
            a.point == b.point && a.weapon_only == b.weapon_only;
    }
    inline bool visit_config(modules::config_visitor& visitor, config& live)
    {
        config candidate = live;
        const bool importing = visitor.importing();
        if (importing) visitor.stage_values(true);
        modules::settings::visit_fields(visitor, candidate, schema());
        if (!importing) return false;
        if (!normalize(candidate)) visitor.reject("Aim Assist: invalid aim point or non-finite settings.");
        visitor.stage_values(false);
        if (visitor.reading() && visitor.valid()) { live = candidate; return true; }
        return false;
    }
}
