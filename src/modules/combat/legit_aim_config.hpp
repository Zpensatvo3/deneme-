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
        float strength = 42.f, fov = 60.f, max_distance = 4.2f;
        aim_point point = aim_point::adaptive;
        bool weapon_only = true;
    };
    inline std::span<const modules::settings::descriptor<config>> schema()
    {
        using d = modules::settings::descriptor<config>;
        using label = modules::settings::label;
        static constexpr label points[]{{"Adaptive"},{"Upper Body"},{"Head"}};
        static constexpr int values[]{4,1,0};
        static const d fields[]{
            d::number("strength", {"Assist Strength"}, &config::strength, 0.f, 100.f, "%.0f%%"),
            d::number("fov", {"FOV"}, &config::fov, 20.f, 120.f, "%.0f deg"),
            d::number("max_distance", {"Range"}, &config::max_distance, 2.5f, 5.f, "%.1f blocks"),
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
