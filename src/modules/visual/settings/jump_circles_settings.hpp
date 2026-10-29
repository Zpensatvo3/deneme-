#pragma once
#include "../../core/setting_schema.hpp"
namespace module_settings
{
    struct jump_circles
    {
        int style = 0, trigger = 0;
        float radius = .85f, lifetime = 1.f, opacity = .8f;
        bool theme_color = true, glow = true;
        float color[4]{.65f,.4f,1.f,1.f};
        bool operator==(const jump_circles&) const = default;
        static std::span<const modules::settings::descriptor<jump_circles>> schema()
        {
            using d = modules::settings::descriptor<jump_circles>;
            static constexpr modules::settings::label styles[]{{"Ring"},{"Double Ring"}};
            static constexpr modules::settings::label triggers[]{{"Jump"},{"Landing"},{"Both"}};
            static const d fields[]{
                d::choice("style",{"Style"},&jump_circles::style,styles),
                d::choice("trigger",{"Show On"},&jump_circles::trigger,triggers),
                d::number("radius",{"Radius"},&jump_circles::radius,.3f,1.5f,"%.2f"),
                d::number("lifetime",{"Lifetime"},&jump_circles::lifetime,.3f,2.f,"%.2f s"),
                d::number("opacity",{"Opacity"},&jump_circles::opacity,.1f,1.f,"%.2f"),
                d::toggle("theme_color",{"Theme Color"},&jump_circles::theme_color),
                d::color("color",{"Color"},&jump_circles::color).when([](const jump_circles& c){return !c.theme_color;}),
                d::toggle("glow",{"Glow"},&jump_circles::glow)
            }; return fields;
        }
    };
}
