#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct watermark
    {
        float scale = modules::catalog::watermark_scale_default, position_x = modules::catalog::watermark_position_x_default, position_y = modules::catalog::watermark_position_y_default;
        bool operator==(const watermark&) const = default;
        static std::span<const modules::settings::descriptor<watermark>> schema()
        {
            using d = modules::settings::descriptor<watermark>;
            static const d fields[]{
                d::number("scale", {"Scale"}, &watermark::scale, modules::catalog::watermark_scale_minimum, modules::catalog::watermark_scale_maximum, "%.2f"),
                d::number("position_x", {"Position X"}, &watermark::position_x, modules::catalog::watermark_position_x_minimum, modules::catalog::watermark_position_x_maximum).hidden(),
                d::number("position_y", {"Position Y"}, &watermark::position_y, modules::catalog::watermark_position_y_minimum, modules::catalog::watermark_position_y_maximum).hidden()
            };
            return fields;
        }
    };
}
