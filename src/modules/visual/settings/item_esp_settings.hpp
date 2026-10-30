#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct item_esp
    {
        bool show_boxes = modules::catalog::item_esp_show_boxes_default, show_names = modules::catalog::item_esp_show_names_default, show_count = modules::catalog::item_esp_show_count_default, show_distance = modules::catalog::item_esp_show_distance_default;
        bool use_theme_color = modules::catalog::item_esp_use_theme_color_default;
        float color[4]{modules::catalog::item_esp_color_default[0], modules::catalog::item_esp_color_default[1], modules::catalog::item_esp_color_default[2], modules::catalog::item_esp_color_default[3]};
        float line_thickness = modules::catalog::item_esp_line_thickness_default, fill_opacity = modules::catalog::item_esp_fill_opacity_default;
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
                d::number("line_thickness", {"Line Thickness"}, &item_esp::line_thickness, modules::catalog::item_esp_line_thickness_minimum, modules::catalog::item_esp_line_thickness_maximum, "%.1f", &item_esp::show_boxes),
                d::number("fill_opacity", {"Fill Opacity"}, &item_esp::fill_opacity, modules::catalog::item_esp_fill_opacity_minimum, modules::catalog::item_esp_fill_opacity_maximum, "%.2f", &item_esp::show_boxes)
            };
            return fields;
        }
    };
}
