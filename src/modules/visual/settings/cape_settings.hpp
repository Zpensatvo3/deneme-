#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct cape
    {
        int style = 1;
        float length = 1.05f, width = .66f, motion = 1.f, opacity = .94f, physics = 1.f;
        bool animate = true, theme_color = true, glow = true;
        bool tint_art = false;
        float color[4]{.65f, .40f, 1.f, 1.f};
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
                d::number("length", {"Length"}, &cape::length, .65f, 1.30f, "%.2f"),
                d::number("width", {"Width"}, &cape::width, .45f, .95f, "%.2f"),
                d::toggle("animate", {"Animation"}, &cape::animate),
                d::number("motion", {"Motion"}, &cape::motion, .25f, 1.75f, "%.2fx", &cape::animate),
                d::number("physics", {"Physics"}, &cape::physics, 0.f, 1.5f, "%.2fx", &cape::animate),
                d::number("opacity", {"Opacity"}, &cape::opacity, .25f, 1.f, "%.2f"),
                d::toggle("theme_color", {"Theme Color"}, &cape::theme_color).when([](const cape& c) { return c.style<8 || c.tint_art; }),
                d::color("color", {"Color"}, &cape::color).when([](const cape& c) { return !c.theme_color && (c.style<8 || c.tint_art); }),
                d::toggle("glow", {"Glow"}, &cape::glow)
            };
            return fields;
        }
    };
}
