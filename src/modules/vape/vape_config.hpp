#pragma once

#include "../core/setting_schema.hpp"

namespace modules { enum class vape_kind { aim }; }

namespace vape_settings
{
    struct config
    {
        bool weapons_only = true, protect_blocks = true;
        float range = 5.f, fov = 180.f, horizontal = 5.f, vertical = 5.f;
        bool aim_vertical = false, strafe_increase = true, require_mouse = true;
        int sort = 0;
        int aim_mode = 0, target_area = 1;
        bool operator==(const config&) const = default;
    };

    inline config defaults(modules::vape_kind)
    {
        config value;
        return value;
    }

    using setting = modules::settings::descriptor<config>;
    using label = modules::settings::label;
    inline std::span<const setting> schema(modules::vape_kind kind)
    {
        static constexpr label engines[]{{"Simple"}, {"Adaptive"}};
        static constexpr label areas[]{{"Center"}, {"Closest"}};
        static constexpr label sorting[]{{"Angle"}, {"Distance"}, {"Health"},
            {"Armor"}, {"Threat"}};
        static const setting aim[]{
            setting::choice("engine", {"Engine"}, &config::aim_mode, engines),
            setting::choice("target_area", {"Target Area"}, &config::target_area, areas),
            setting::number("horizontal_speed", {"Horizontal Speed"}, &config::horizontal, .1f, 20.f),
            setting::toggle("aim_vertical", {"Vertical Aim"}, &config::aim_vertical),
            setting::number("vertical_speed", {"Vertical Speed"}, &config::vertical, .1f, 20.f, "%.1f", &config::aim_vertical),
            setting::number("range", {"Range"}, &config::range, 1.f, 8.f, "%.1f m"),
            setting::number("fov", {"FOV"}, &config::fov, 10.f, 360.f, "%.0f"),
            setting::choice("sort", {"Priority"}, &config::sort, sorting),
            setting::toggle("strafe_increase", {"Strafe Increase"}, &config::strafe_increase),
            setting::toggle("require_mouse", {"On Click Only"}, &config::require_mouse),
            setting::toggle("weapons_only", {"Weapons Only"}, &config::weapons_only),
            setting::toggle("protect_blocks", {"Mining Protection"}, &config::protect_blocks),
        };
        switch (kind)
        {
        case modules::vape_kind::aim: return aim;
        }
        return {};
    }

    inline bool normalize(modules::vape_kind kind, config& candidate)
    {
        return modules::settings::normalize(candidate, schema(kind));
    }
    inline bool visit(modules::config_visitor& visitor, modules::vape_kind kind, config& live)
    {
        return modules::settings::visit(visitor, live, schema(kind), "Vape: invalid or non-finite settings.");
    }
}
