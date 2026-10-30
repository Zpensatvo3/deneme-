#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct player_esp
    {
        int esp_mode = modules::catalog::player_esp_esp_mode_default;
        float box_thickness = modules::catalog::player_esp_box_thickness_default;
        float fill_opacity = modules::catalog::player_esp_fill_opacity_default;
        bool show_names = modules::catalog::player_esp_show_names_default;
        bool show_health = modules::catalog::player_esp_show_health_default;
        bool show_distance = modules::catalog::player_esp_show_distance_default;
        float fov_value = modules::catalog::player_esp_fov_default;
        bool use_game_fov = modules::catalog::player_esp_use_game_fov_default;
        bool use_theme_color = modules::catalog::player_esp_use_theme_color_default;
        float esp_color[4] = {modules::catalog::player_esp_esp_color_default[0], modules::catalog::player_esp_esp_color_default[1], modules::catalog::player_esp_esp_color_default[2], modules::catalog::player_esp_esp_color_default[3]};
        bool operator==(const player_esp&) const = default;
        static std::span<const modules::settings::descriptor<player_esp>> schema()
        {
            using model = player_esp;
            using setting = modules::settings::descriptor<model>;
            static constexpr modules::settings::label esp_mode_labels[]{{"2D Box"},{"Corners"},{"3D Box"}};
            static const setting fields[]{
                setting::choice("esp_mode", {"Style"}, &model::esp_mode, esp_mode_labels),
                setting::number("fill_opacity", {"Fill Opacity"}, &model::fill_opacity, modules::catalog::player_esp_fill_opacity_minimum, modules::catalog::player_esp_fill_opacity_maximum, "%.2f").when([](const model& c) { return c.esp_mode == 2; }),
                setting::number("box_thickness", {"Thickness"}, &model::box_thickness, modules::catalog::player_esp_box_thickness_minimum, modules::catalog::player_esp_box_thickness_maximum, "%.1f"),
                setting::toggle("use_theme_color", {"Use Theme Color"}, &model::use_theme_color),
                setting::color("esp_color", {"ESP Color"}, &model::esp_color).when([](const model& c) { return !c.use_theme_color; }),
                setting::toggle("show_names", {"Show Names"}, &model::show_names),
                setting::toggle("show_health", {"Show Health"}, &model::show_health),
                setting::toggle("show_distance", {"Show Distance"}, &model::show_distance),
                setting::toggle("use_game_fov", {"Use Game FOV"}, &model::use_game_fov),
                setting::number("fov", {"FOV"}, &model::fov_value, modules::catalog::player_esp_fov_minimum, modules::catalog::player_esp_fov_maximum, "%.0f").when([](const model& c) { return !c.use_game_fov; }),
            };
            return fields;
        }
    };
}
