#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct watermark
    {
        float scale = 1.f, position_x = 10.f, position_y = 10.f;
        bool operator==(const watermark&) const = default;
        static std::span<const modules::settings::descriptor<watermark>> schema()
        {
            using d = modules::settings::descriptor<watermark>;
            static const d fields[]{
                d::number("scale", {"Scale"}, &watermark::scale, .5f, 2.f, "%.2f"),
                d::number("position_x", {"Position X"}, &watermark::position_x, -1000000.f, 1000000.f).hidden(),
                d::number("position_y", {"Position Y"}, &watermark::position_y, -1000000.f, 1000000.f).hidden()
            };
            return fields;
        }
    };
}
