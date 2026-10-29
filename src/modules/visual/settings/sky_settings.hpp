#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct sky
    {
        int style = 0;
        int mode = 1, panorama = 3;
        float exposure = 0.f, rotation = 0.f;
        bool drift = false;
        float strength = .85f;
        bool match_fog = true;
        float color[4]{.65f,.38f,.95f,1.f};
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
                d::number("exposure", {"Brightness"}, &sky::exposure, -2.f, 2.f, "%+.1f").when([](const sky& c) { return c.mode==1; }),
                d::number("rotation", {"Rotation"}, &sky::rotation, 0.f, 360.f, "%.0f°").when([](const sky& c) { return c.mode==1; }),
                d::toggle("drift", {"Slow Rotation"}, &sky::drift).when([](const sky& c) { return c.mode==1; }),
                d::choice("style", {"Style"}, &sky::style, styles).when([](const sky& c) { return c.mode==0; }),
                d::number("strength", {"Strength"}, &sky::strength, 0.f, 1.f, "%.2f"),
                d::color("color", {"Sky Color"}, &sky::color).when([](const sky& c) { return c.mode==0 && c.style==4; }),
                d::toggle("match_fog", {"Match Fog"}, &sky::match_fog)
            };
            return fields;
        }
    };
}
