#pragma once
#include "../../core/setting_schema.hpp"
namespace module_settings
{
    struct hit_particles
    {
        int style = modules::catalog::hit_particles_style_default, count = modules::catalog::hit_particles_count_default;
        float lifetime = modules::catalog::hit_particles_lifetime_default, size = modules::catalog::hit_particles_size_default, opacity = modules::catalog::hit_particles_opacity_default;
        bool theme_color = modules::catalog::hit_particles_theme_color_default, glow = modules::catalog::hit_particles_glow_default;
        float color[4]{modules::catalog::hit_particles_color_default[0], modules::catalog::hit_particles_color_default[1], modules::catalog::hit_particles_color_default[2], modules::catalog::hit_particles_color_default[3]};
        bool operator==(const hit_particles&) const = default;
        static std::span<const modules::settings::descriptor<hit_particles>> schema()
        {
            using d = modules::settings::descriptor<hit_particles>;
            static constexpr modules::settings::label styles[]{{"Stars"},{"Sparks"},{"Rings"}};
            static const d fields[]{
                d::choice("style",{"Style"},&hit_particles::style,styles),
                d::integer("count",{"Particles"},&hit_particles::count,modules::catalog::hit_particles_count_minimum,modules::catalog::hit_particles_count_maximum),
                d::number("lifetime",{"Lifetime"},&hit_particles::lifetime,modules::catalog::hit_particles_lifetime_minimum,modules::catalog::hit_particles_lifetime_maximum,"%.2f s"),
                d::number("size",{"Size"},&hit_particles::size,modules::catalog::hit_particles_size_minimum,modules::catalog::hit_particles_size_maximum,"%.2fx"),
                d::number("opacity",{"Opacity"},&hit_particles::opacity,modules::catalog::hit_particles_opacity_minimum,modules::catalog::hit_particles_opacity_maximum,"%.2f"),
                d::toggle("theme_color",{"Theme Color"},&hit_particles::theme_color),
                d::color("color",{"Color"},&hit_particles::color).when([](const hit_particles& c){return !c.theme_color;}),
                d::toggle("glow",{"Glow"},&hit_particles::glow)
            }; return fields;
        }
    };
}
