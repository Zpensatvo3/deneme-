#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct china_hat
    {
        int style = modules::catalog::china_hat_style_default;
        float radius = modules::catalog::china_hat_radius_default, height = modules::catalog::china_hat_height_default, offset = modules::catalog::china_hat_offset_default, opacity = modules::catalog::china_hat_opacity_default;
        bool theme_color = modules::catalog::china_hat_theme_color_default, glow = modules::catalog::china_hat_glow_default;
        float color[4]{modules::catalog::china_hat_color_default[0], modules::catalog::china_hat_color_default[1], modules::catalog::china_hat_color_default[2], modules::catalog::china_hat_color_default[3]};
        bool operator==(const china_hat&) const = default;
        static std::span<const modules::settings::descriptor<china_hat>> schema()
        {
            using d = modules::settings::descriptor<china_hat>;
            static constexpr modules::settings::label styles[]{
                {"Classic"}, {"Wide Brim"}, {"Pagoda"}
            };
            static const d fields[]{
                d::choice("style", {"Style"}, &china_hat::style, styles),
                d::number("radius", {"Width"}, &china_hat::radius, modules::catalog::china_hat_radius_minimum, modules::catalog::china_hat_radius_maximum, "%.2f"),
                d::number("height", {"Height"}, &china_hat::height, modules::catalog::china_hat_height_minimum, modules::catalog::china_hat_height_maximum, "%.2f"),
                d::number("offset", {"Head Offset"}, &china_hat::offset, modules::catalog::china_hat_offset_minimum, modules::catalog::china_hat_offset_maximum, "%.2f"),
                d::number("opacity", {"Opacity"}, &china_hat::opacity, modules::catalog::china_hat_opacity_minimum, modules::catalog::china_hat_opacity_maximum, "%.2f"),
                d::toggle("theme_color", {"Theme Color"}, &china_hat::theme_color),
                d::color("color", {"Color"}, &china_hat::color).when([](const china_hat& c) { return !c.theme_color; }),
                d::toggle("glow", {"Glow"}, &china_hat::glow)
            };
            return fields;
        }
    };
}
