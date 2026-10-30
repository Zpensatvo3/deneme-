#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct chams
    {
        float opacity = modules::catalog::chams_opacity_default;
        bool glow = modules::catalog::chams_glow_default;
        float glow_strength = modules::catalog::chams_glow_strength_default, glow_radius = modules::catalog::chams_glow_radius_default;
        bool through_walls = modules::catalog::chams_through_walls_default, theme_color = modules::catalog::chams_theme_color_default, ignore_friends = modules::catalog::chams_ignore_friends_default;
        float visible_color[4]{modules::catalog::chams_visible_color_default[0], modules::catalog::chams_visible_color_default[1], modules::catalog::chams_visible_color_default[2], modules::catalog::chams_visible_color_default[3]};
        float hidden_color[4]{modules::catalog::chams_hidden_color_default[0], modules::catalog::chams_hidden_color_default[1], modules::catalog::chams_hidden_color_default[2], modules::catalog::chams_hidden_color_default[3]};
        bool operator==(const chams&) const = default;
        static std::span<const modules::settings::descriptor<chams>> schema()
        {
            using d = modules::settings::descriptor<chams>;
            static const d fields[]{
                d::toggle("theme_color", {"Theme Color"}, &chams::theme_color),
                d::color("visible_color", {"Visible Color"}, &chams::visible_color).when([](const chams& c) { return !c.theme_color; }),
                d::toggle("through_walls", {"Through Walls"}, &chams::through_walls),
                d::color("hidden_color", {"Hidden Color"}, &chams::hidden_color).when([](const chams& c) { return c.through_walls; }),
                d::number("opacity", {"Opacity"}, &chams::opacity, modules::catalog::chams_opacity_minimum, modules::catalog::chams_opacity_maximum, "%.2f"),
                d::toggle("ignore_friends", {"Ignore Friends"}, &chams::ignore_friends),
                d::toggle("glow", {"Glow"}, &chams::glow),
                d::number("glow_strength", {"Glow Strength"}, &chams::glow_strength, modules::catalog::chams_glow_strength_minimum, modules::catalog::chams_glow_strength_maximum, "%.2f", &chams::glow),
                d::number("glow_radius", {"Glow Spread"}, &chams::glow_radius, modules::catalog::chams_glow_radius_minimum, modules::catalog::chams_glow_radius_maximum, "%.0f px", &chams::glow)
            };
            return fields;
        }
    };
}
