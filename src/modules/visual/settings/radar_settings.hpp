#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct radar
    {
        float radar_size = 150.0f;
        float radar_range = 48.0f;
        float position_x = 18.0f;
        float position_y = 92.0f;
        float background_opacity = 0.78f;
        bool rotate_with_player = true;
        bool show_names = false;
        bool show_distance_rings = true;
        bool show_cardinals = true;
        bool show_player_count = true;
        bool operator==(const radar&) const = default;
        static std::span<const modules::settings::descriptor<radar>> schema()
        {
            using model = radar;
            using setting = modules::settings::descriptor<model>;
            static const setting fields[]{
                setting::number("size", {"Radar Size"}, &model::radar_size, 120.0f, 260.0f, "%.0f px"),
                setting::number("range", {"Range"}, &model::radar_range, 8.0f, 128.0f, "%.0f blocks"),
                setting::number("position_x", {"Position X"}, &model::position_x, -1000000.f, 1000000.f, "%.0f px").display_limits(4.f, 1200.f),
                setting::number("position_y", {"Position Y"}, &model::position_y, -1000000.f, 1000000.f, "%.0f px").display_limits(4.f, 800.f),
                setting::number("background_opacity", {"Background"}, &model::background_opacity, 0.20f, 1.0f, "%.2f"),
                setting::toggle("rotate_with_player", {"Rotate with Player"}, &model::rotate_with_player),
                setting::toggle("show_names", {"Show Names"}, &model::show_names),
                setting::toggle("show_distance_rings", {"Distance Rings"}, &model::show_distance_rings),
                setting::toggle("show_cardinals", {"Cardinal Directions"}, &model::show_cardinals),
                setting::toggle("show_player_count", {"Player Count"}, &model::show_player_count),
            };
            return fields;
        }
    };
}
