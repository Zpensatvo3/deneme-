#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct sky
    {
        int style = modules::catalog::sky_style_default;
        int mode = modules::catalog::sky_mode_default, panorama = modules::catalog::sky_panorama_default;
        float exposure = modules::catalog::sky_exposure_default, rotation = modules::catalog::sky_rotation_default;
        bool drift = modules::catalog::sky_drift_default;
        float strength = modules::catalog::sky_strength_default;
        bool match_fog = modules::catalog::sky_match_fog_default;
        float color[4]{modules::catalog::sky_color_default[0], modules::catalog::sky_color_default[1], modules::catalog::sky_color_default[2], modules::catalog::sky_color_default[3]};
        bool operator==(const sky&) const = default;
        static std::span<const modules::settings::descriptor<sky>> schema()
        {
            using d = modules::settings::descriptor<sky>;
            static constexpr modules::settings::label styles[]{
                {"Theme"}, {"Sunset"}, {"Violet"},
                {"Ice"}, {"Custom"}
            };
            static constexpr modules::settings::label modes[]{{"Color"},{"Panorama"}};
            static constexpr modules::settings::label panoramas[]{
                {"Starry Night"}, {"Milky Way"}, {"Cloudy Sunrise"},
                {"Candy Dusk"}, {"Nebula"}, {"Aurora"}
            };
            static const d fields[]{
                d::choice("mode", {"Mode"}, &sky::mode, modes),
                d::choice("panorama", {"Panorama"}, &sky::panorama, panoramas).when([](const sky& c) { return c.mode==1; }),
                d::number("exposure", {"Brightness"}, &sky::exposure, modules::catalog::sky_exposure_minimum, modules::catalog::sky_exposure_maximum, "%+.1f").when([](const sky& c) { return c.mode==1; }),
                d::number("rotation", {"Rotation"}, &sky::rotation, modules::catalog::sky_rotation_minimum, modules::catalog::sky_rotation_maximum, "%.0f°").when([](const sky& c) { return c.mode==1; }),
                d::toggle("drift", {"Slow Rotation"}, &sky::drift).when([](const sky& c) { return c.mode==1; }),
                d::choice("style", {"Style"}, &sky::style, styles).when([](const sky& c) { return c.mode==0; }),
                d::number("strength", {"Strength"}, &sky::strength, modules::catalog::sky_strength_minimum, modules::catalog::sky_strength_maximum, "%.2f"),
                d::color("color", {"Sky Color"}, &sky::color).when([](const sky& c) { return c.mode==0 && c.style==4; }),
                d::toggle("match_fog", {"Match Fog"}, &sky::match_fog)
            };
            return fields;
        }
    };
}
