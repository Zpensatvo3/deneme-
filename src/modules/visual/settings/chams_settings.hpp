#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct chams
    {
        float opacity = .65f;
        bool glow = true;
        float glow_strength = .85f, glow_radius = 8.f;
        bool through_walls = true, theme_color = true, ignore_friends = true;
        float visible_color[4]{.60f, .32f, 1.f, 1.f};
        float hidden_color[4]{.94f, .25f, .36f, 1.f};
        bool operator==(const chams&) const = default;
        static std::span<const modules::settings::descriptor<chams>> schema()
        {
            using d = modules::settings::descriptor<chams>;
            static const d fields[]{
                d::toggle("theme_color", {"Theme Color"}, &chams::theme_color),
                d::color("visible_color", {"Visible Color"}, &chams::visible_color).when([](const chams& c) { return !c.theme_color; }),
                d::toggle("through_walls", {"Through Walls"}, &chams::through_walls),
                d::color("hidden_color", {"Hidden Color"}, &chams::hidden_color).when([](const chams& c) { return c.through_walls; }),
                d::number("opacity", {"Opacity"}, &chams::opacity, 0.f, 1.f, "%.2f"),
                d::toggle("ignore_friends", {"Ignore Friends"}, &chams::ignore_friends),
                d::toggle("glow", {"Glow"}, &chams::glow),
                d::number("glow_strength", {"Glow Strength"}, &chams::glow_strength, 0.f, 1.f, "%.2f", &chams::glow),
                d::number("glow_radius", {"Glow Spread"}, &chams::glow_radius, 2.f, 20.f, "%.0f px", &chams::glow)
            };
            return fields;
        }
    };
}
