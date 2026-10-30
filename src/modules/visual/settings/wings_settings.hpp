#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct wings
    {
        int style = modules::catalog::wings_style_default;
        float size = modules::catalog::wings_size_default, opacity = modules::catalog::wings_opacity_default, speed = modules::catalog::wings_speed_default, physics = modules::catalog::wings_physics_default;
        bool animate = modules::catalog::wings_animate_default, theme_color = modules::catalog::wings_theme_color_default, glow = modules::catalog::wings_glow_default;
        bool detailed_art = modules::catalog::wings_detailed_art_default, tint_art = modules::catalog::wings_tint_art_default;
        float color[4]{modules::catalog::wings_color_default[0], modules::catalog::wings_color_default[1], modules::catalog::wings_color_default[2], modules::catalog::wings_color_default[3]};
        bool operator==(const wings&) const = default;
        static std::span<const modules::settings::descriptor<wings>> schema()
        {
            using d = modules::settings::descriptor<wings>;
            static constexpr modules::settings::label styles[]{
                {"Dragon"}, {"Angel"}, {"Butterfly"}
            };
            static const d fields[]{
                d::choice("style", {"Style"}, &wings::style, styles),
                d::toggle("detailed_art", {"Detailed Art"}, &wings::detailed_art),
                d::toggle("tint_art", {"Tint Artwork"}, &wings::tint_art).when([](const wings& c) { return c.detailed_art; }),
                d::number("size", {"Size"}, &wings::size, modules::catalog::wings_size_minimum, modules::catalog::wings_size_maximum, "%.2fx"),
                d::number("opacity", {"Opacity"}, &wings::opacity, modules::catalog::wings_opacity_minimum, modules::catalog::wings_opacity_maximum, "%.2f"),
                d::toggle("animate", {"Flapping"}, &wings::animate),
                d::number("speed", {"Flap Speed"}, &wings::speed, modules::catalog::wings_speed_minimum, modules::catalog::wings_speed_maximum, "%.2fx", &wings::animate),
                d::number("physics", {"Physics"}, &wings::physics, modules::catalog::wings_physics_minimum, modules::catalog::wings_physics_maximum, "%.2fx", &wings::animate),
                d::toggle("theme_color", {"Theme Color"}, &wings::theme_color).when([](const wings& c) { return !c.detailed_art || c.tint_art; }),
                d::color("color", {"Color"}, &wings::color).when([](const wings& c) { return !c.theme_color && (!c.detailed_art || c.tint_art); }),
                d::toggle("glow", {"Glow"}, &wings::glow)
            };
            return fields;
        }
    };
}
