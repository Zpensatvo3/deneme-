#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct wings
    {
        int style = 0;
        float size = 1.f, opacity = .85f, speed = 1.f, physics = 1.f;
        bool animate = true, theme_color = true, glow = true;
        bool detailed_art = true, tint_art = false;
        float color[4]{.65f, .40f, 1.f, 1.f};
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
                d::number("size", {"Size"}, &wings::size, .5f, 1.6f, "%.2fx"),
                d::number("opacity", {"Opacity"}, &wings::opacity, .1f, 1.f, "%.2f"),
                d::toggle("animate", {"Flapping"}, &wings::animate),
                d::number("speed", {"Flap Speed"}, &wings::speed, .25f, 2.f, "%.2fx", &wings::animate),
                d::number("physics", {"Physics"}, &wings::physics, 0.f, 1.5f, "%.2fx", &wings::animate),
                d::toggle("theme_color", {"Theme Color"}, &wings::theme_color).when([](const wings& c) { return !c.detailed_art || c.tint_art; }),
                d::color("color", {"Color"}, &wings::color).when([](const wings& c) { return !c.theme_color && (!c.detailed_art || c.tint_art); }),
                d::toggle("glow", {"Glow"}, &wings::glow)
            };
            return fields;
        }
    };
}
