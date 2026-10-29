#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct item_esp
    {
        bool show_boxes = true, show_names = true, show_count = true, show_distance = true;
        bool use_theme_color = true;
        float color[4]{1.f, .8f, .3f, 1.f};
        float line_thickness = 1.4f, fill_opacity = .08f;
        bool operator==(const item_esp&) const = default;
        static std::span<const modules::settings::descriptor<item_esp>> schema()
        {
            using d = modules::settings::descriptor<item_esp>;
            static const d fields[]{
                d::toggle("show_boxes", {"Show Boxes"}, &item_esp::show_boxes),
                d::toggle("show_names", {"Show Names"}, &item_esp::show_names),
                d::toggle("show_count", {"Show Count"}, &item_esp::show_count),
                d::toggle("show_distance", {"Show Distance"}, &item_esp::show_distance),
                d::toggle("use_theme_color", {"Theme Color"}, &item_esp::use_theme_color),
                d::color("color", {"Item Color"}, &item_esp::color).when([](const item_esp& c) { return !c.use_theme_color; }),
                d::number("line_thickness", {"Line Thickness"}, &item_esp::line_thickness, .5f, 3.f, "%.1f", &item_esp::show_boxes),
                d::number("fill_opacity", {"Fill Opacity"}, &item_esp::fill_opacity, 0.f, .35f, "%.2f", &item_esp::show_boxes)
            };
            return fields;
        }
    };
}
