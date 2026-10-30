#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct cape
    {
        int style = modules::catalog::cape_style_default;
        float length = modules::catalog::cape_length_default, width = modules::catalog::cape_width_default, motion = modules::catalog::cape_motion_default, opacity = modules::catalog::cape_opacity_default, physics = modules::catalog::cape_physics_default;
        bool animate = modules::catalog::cape_animate_default, theme_color = modules::catalog::cape_theme_color_default, glow = modules::catalog::cape_glow_default;
        bool tint_art = modules::catalog::cape_tint_art_default;
        float color[4]{modules::catalog::cape_color_default[0], modules::catalog::cape_color_default[1], modules::catalog::cape_color_default[2], modules::catalog::cape_color_default[3]};
        bool operator==(const cape&) const = default;
        static std::span<const modules::settings::descriptor<cape>> schema()
        {
            using d = modules::settings::descriptor<cape>;
            static constexpr modules::settings::label styles[]{
                {"Plain"}, {"Bordered"}, {"Emblem"}, {"Split"},
                {"Inferno"}, {"Constellation"}, {"Circuit"}, {"Wings"}, {"Phoenix"}, {"Astral Dragon"}
            };
            static const d fields[]{
                d::choice("style", {"Style"}, &cape::style, styles),
                d::toggle("tint_art", {"Tint Artwork"}, &cape::tint_art).when([](const cape& c) { return c.style>=8; }),
                d::number("length", {"Length"}, &cape::length, modules::catalog::cape_length_minimum, modules::catalog::cape_length_maximum, "%.2f"),
                d::number("width", {"Width"}, &cape::width, modules::catalog::cape_width_minimum, modules::catalog::cape_width_maximum, "%.2f"),
                d::toggle("animate", {"Animation"}, &cape::animate),
                d::number("motion", {"Motion"}, &cape::motion, modules::catalog::cape_motion_minimum, modules::catalog::cape_motion_maximum, "%.2fx", &cape::animate),
                d::number("physics", {"Physics"}, &cape::physics, modules::catalog::cape_physics_minimum, modules::catalog::cape_physics_maximum, "%.2fx", &cape::animate),
                d::number("opacity", {"Opacity"}, &cape::opacity, modules::catalog::cape_opacity_minimum, modules::catalog::cape_opacity_maximum, "%.2f"),
                d::toggle("theme_color", {"Theme Color"}, &cape::theme_color).when([](const cape& c) { return c.style<8 || c.tint_art; }),
                d::color("color", {"Color"}, &cape::color).when([](const cape& c) { return !c.theme_color && (c.style<8 || c.tint_art); }),
                d::toggle("glow", {"Glow"}, &cape::glow)
            };
            return fields;
        }
    };
}
