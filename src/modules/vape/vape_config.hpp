#pragma once

#include "../core/setting_schema.hpp"

namespace modules { enum class vape_kind { aim }; }

namespace vape_settings
{
    struct config
    {
        bool weapons_only = modules::catalog::vape_aim_assist_weapons_only_default, protect_blocks = modules::catalog::vape_aim_assist_protect_blocks_default;
        float range = modules::catalog::vape_aim_assist_range_default, fov = modules::catalog::vape_aim_assist_fov_default, horizontal = modules::catalog::vape_aim_assist_horizontal_speed_default, vertical = modules::catalog::vape_aim_assist_vertical_speed_default;
        bool aim_vertical = modules::catalog::vape_aim_assist_aim_vertical_default, strafe_increase = modules::catalog::vape_aim_assist_strafe_increase_default, require_mouse = modules::catalog::vape_aim_assist_require_mouse_default;
        int sort = modules::catalog::vape_aim_assist_sort_default;
        int aim_mode = modules::catalog::vape_aim_assist_engine_default, target_area = modules::catalog::vape_aim_assist_target_area_default;
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
            setting::number("horizontal_speed", {"Horizontal Speed"}, &config::horizontal, modules::catalog::vape_aim_assist_horizontal_speed_minimum, modules::catalog::vape_aim_assist_horizontal_speed_maximum),
            setting::toggle("aim_vertical", {"Vertical Aim"}, &config::aim_vertical),
            setting::number("vertical_speed", {"Vertical Speed"}, &config::vertical, modules::catalog::vape_aim_assist_vertical_speed_minimum, modules::catalog::vape_aim_assist_vertical_speed_maximum, "%.1f", &config::aim_vertical),
            setting::number("range", {"Range"}, &config::range, modules::catalog::vape_aim_assist_range_minimum, modules::catalog::vape_aim_assist_range_maximum, "%.1f m"),
            setting::number("fov", {"FOV"}, &config::fov, modules::catalog::vape_aim_assist_fov_minimum, modules::catalog::vape_aim_assist_fov_maximum, "%.0f"),
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
