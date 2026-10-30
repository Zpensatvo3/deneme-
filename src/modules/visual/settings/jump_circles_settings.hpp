#pragma once
#include "../../core/setting_schema.hpp"
namespace module_settings
{
    struct jump_circles
    {
        int style = modules::catalog::jump_circles_style_default, trigger = modules::catalog::jump_circles_trigger_default;
        float radius = modules::catalog::jump_circles_radius_default, lifetime = modules::catalog::jump_circles_lifetime_default, opacity = modules::catalog::jump_circles_opacity_default;
        bool theme_color = modules::catalog::jump_circles_theme_color_default, glow = modules::catalog::jump_circles_glow_default;
        float color[4]{modules::catalog::jump_circles_color_default[0], modules::catalog::jump_circles_color_default[1], modules::catalog::jump_circles_color_default[2], modules::catalog::jump_circles_color_default[3]};
        bool operator==(const jump_circles&) const = default;
        static std::span<const modules::settings::descriptor<jump_circles>> schema()
        {
            using d = modules::settings::descriptor<jump_circles>;
            static constexpr modules::settings::label styles[]{{"Ring"},{"Double Ring"}};
            static constexpr modules::settings::label triggers[]{{"Jump"},{"Landing"},{"Both"}};
            static const d fields[]{
                d::choice("style",{"Style"},&jump_circles::style,styles),
                d::choice("trigger",{"Show On"},&jump_circles::trigger,triggers),
                d::number("radius",{"Radius"},&jump_circles::radius,modules::catalog::jump_circles_radius_minimum,modules::catalog::jump_circles_radius_maximum,"%.2f"),
                d::number("lifetime",{"Lifetime"},&jump_circles::lifetime,modules::catalog::jump_circles_lifetime_minimum,modules::catalog::jump_circles_lifetime_maximum,"%.2f s"),
                d::number("opacity",{"Opacity"},&jump_circles::opacity,modules::catalog::jump_circles_opacity_minimum,modules::catalog::jump_circles_opacity_maximum,"%.2f"),
                d::toggle("theme_color",{"Theme Color"},&jump_circles::theme_color),
                d::color("color",{"Color"},&jump_circles::color).when([](const jump_circles& c){return !c.theme_color;}),
                d::toggle("glow",{"Glow"},&jump_circles::glow)
            }; return fields;
        }
    };
}
