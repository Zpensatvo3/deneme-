#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct trails
    {
        int mode = modules::catalog::trails_mode_default;
        float lifetime = modules::catalog::trails_lifetime_default, density = modules::catalog::trails_density_default, size = modules::catalog::trails_size_default, opacity = modules::catalog::trails_opacity_default;
        bool theme_color = modules::catalog::trails_theme_color_default, glow = modules::catalog::trails_glow_default;
        float color[4]{modules::catalog::trails_color_default[0], modules::catalog::trails_color_default[1], modules::catalog::trails_color_default[2], modules::catalog::trails_color_default[3]};
        bool operator==(const trails&) const = default;
        static std::span<const modules::settings::descriptor<trails>> schema()
        {
            using d = modules::settings::descriptor<trails>;
            static constexpr modules::settings::label modes[]{
                {"Particles"}, {"Footprints"}, {"Both"}
            };
            static const d fields[]{
                d::choice("mode", {"Trail"}, &trails::mode, modes),
                d::number("lifetime", {"Lifetime"}, &trails::lifetime, modules::catalog::trails_lifetime_minimum, modules::catalog::trails_lifetime_maximum, "%.1f s"),
                d::number("density", {"Particle Density"}, &trails::density, modules::catalog::trails_density_minimum, modules::catalog::trails_density_maximum, "%.1fx").when([](const trails& c) { return c.mode!=1; }),
                d::number("size", {"Size"}, &trails::size, modules::catalog::trails_size_minimum, modules::catalog::trails_size_maximum, "%.2fx"),
                d::number("opacity", {"Opacity"}, &trails::opacity, modules::catalog::trails_opacity_minimum, modules::catalog::trails_opacity_maximum, "%.2f"),
                d::toggle("theme_color", {"Theme Color"}, &trails::theme_color),
                d::color("color", {"Color"}, &trails::color).when([](const trails& c) { return !c.theme_color; }),
                d::toggle("glow", {"Glow"}, &trails::glow)
            };
            return fields;
        }
    };
}
