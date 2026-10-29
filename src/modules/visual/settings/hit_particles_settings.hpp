#pragma once
#include "../../core/setting_schema.hpp"
namespace module_settings
{
    struct hit_particles
    {
        int style = 0, count = 14;
        float lifetime = .65f, size = 1.f, opacity = .9f;
        bool theme_color = true, glow = true;
        float color[4]{.65f,.4f,1.f,1.f};
        bool operator==(const hit_particles&) const = default;
        static std::span<const modules::settings::descriptor<hit_particles>> schema()
        {
            using d = modules::settings::descriptor<hit_particles>;
            static constexpr modules::settings::label styles[]{{"Stars"},{"Sparks"},{"Rings"}};
            static const d fields[]{
                d::choice("style",{"Style"},&hit_particles::style,styles),
                d::integer("count",{"Particles"},&hit_particles::count,4,24),
                d::number("lifetime",{"Lifetime"},&hit_particles::lifetime,.2f,1.5f,"%.2f s"),
                d::number("size",{"Size"},&hit_particles::size,.5f,2.f,"%.2fx"),
                d::number("opacity",{"Opacity"},&hit_particles::opacity,.1f,1.f,"%.2f"),
                d::toggle("theme_color",{"Theme Color"},&hit_particles::theme_color),
                d::color("color",{"Color"},&hit_particles::color).when([](const hit_particles& c){return !c.theme_color;}),
                d::toggle("glow",{"Glow"},&hit_particles::glow)
            }; return fields;
        }
    };
}
