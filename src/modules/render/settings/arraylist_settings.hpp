#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct arraylist
    {
        float scale = 1.12f, position_x = 10.f, position_y = 10.f;
        bool bar_glow = true, text_shadow = true, show_keybinds = true;
        float bg_opacity = .72f, bar_width = 2.f;
        bool operator==(const arraylist&) const = default;
        static std::span<const modules::settings::descriptor<arraylist>> schema()
        {
            using d = modules::settings::descriptor<arraylist>;
            static const d fields[]{
                d::number("scale", {"Scale"}, &arraylist::scale, .5f, 2.f, "%.2f"),
                d::number("position_x", {"Position X"}, &arraylist::position_x, -1000000.f, 1000000.f).hidden(),
                d::number("position_y", {"Position Y"}, &arraylist::position_y, -1000000.f, 1000000.f).hidden(),
                d::number("bg_opacity", {"Background Opacity"}, &arraylist::bg_opacity, .2f, 1.f, "%.2f"),
                d::number("bar_width", {"Bar Width"}, &arraylist::bar_width, 1.5f, 6.f, "%.1f px"),
                d::toggle("bar_glow", {"Bar Neon Glow"}, &arraylist::bar_glow),
                d::toggle("text_shadow", {"Text Shadow"}, &arraylist::text_shadow),
                d::toggle("show_keybinds", {"Show Keybinds"}, &arraylist::show_keybinds)
            };
            return fields;
        }
    };
}
