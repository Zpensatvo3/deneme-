#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct arraylist
    {
        float scale = modules::catalog::arraylist_scale_default, position_x = modules::catalog::arraylist_position_x_default, position_y = modules::catalog::arraylist_position_y_default;
        bool bar_glow = modules::catalog::arraylist_bar_glow_default, text_shadow = modules::catalog::arraylist_text_shadow_default, show_keybinds = modules::catalog::arraylist_show_keybinds_default;
        float bg_opacity = modules::catalog::arraylist_bg_opacity_default, bar_width = modules::catalog::arraylist_bar_width_default;
        bool operator==(const arraylist&) const = default;
        static std::span<const modules::settings::descriptor<arraylist>> schema()
        {
            using d = modules::settings::descriptor<arraylist>;
            static const d fields[]{
                d::number("scale", {"Scale"}, &arraylist::scale, modules::catalog::arraylist_scale_minimum, modules::catalog::arraylist_scale_maximum, "%.2f"),
                d::number("position_x", {"Position X"}, &arraylist::position_x, modules::catalog::arraylist_position_x_minimum, modules::catalog::arraylist_position_x_maximum).hidden(),
                d::number("position_y", {"Position Y"}, &arraylist::position_y, modules::catalog::arraylist_position_y_minimum, modules::catalog::arraylist_position_y_maximum).hidden(),
                d::number("bg_opacity", {"Background Opacity"}, &arraylist::bg_opacity, modules::catalog::arraylist_bg_opacity_minimum, modules::catalog::arraylist_bg_opacity_maximum, "%.2f"),
                d::number("bar_width", {"Bar Width"}, &arraylist::bar_width, modules::catalog::arraylist_bar_width_minimum, modules::catalog::arraylist_bar_width_maximum, "%.1f px"),
                d::toggle("bar_glow", {"Bar Neon Glow"}, &arraylist::bar_glow),
                d::toggle("text_shadow", {"Text Shadow"}, &arraylist::text_shadow),
                d::toggle("show_keybinds", {"Show Keybinds"}, &arraylist::show_keybinds)
            };
            return fields;
        }
    };
}
