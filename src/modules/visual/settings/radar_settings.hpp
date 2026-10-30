#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct radar
    {
        float radar_size = modules::catalog::radar_size_default;
        float radar_range = modules::catalog::radar_range_default;
        float position_x = modules::catalog::radar_position_x_default;
        float position_y = modules::catalog::radar_position_y_default;
        float background_opacity = modules::catalog::radar_background_opacity_default;
        bool rotate_with_player = modules::catalog::radar_rotate_with_player_default;
        bool show_names = modules::catalog::radar_show_names_default;
        bool show_distance_rings = modules::catalog::radar_show_distance_rings_default;
        bool show_cardinals = modules::catalog::radar_show_cardinals_default;
        bool show_player_count = modules::catalog::radar_show_player_count_default;
        bool operator==(const radar&) const = default;
        static std::span<const modules::settings::descriptor<radar>> schema()
        {
            using model = radar;
            using setting = modules::settings::descriptor<model>;
            static const setting fields[]{
                setting::number("size", {"Radar Size"}, &model::radar_size, modules::catalog::radar_size_minimum, modules::catalog::radar_size_maximum, "%.0f px"),
                setting::number("range", {"Range"}, &model::radar_range, modules::catalog::radar_range_minimum, modules::catalog::radar_range_maximum, "%.0f blocks"),
                setting::number("position_x", {"Position X"}, &model::position_x, modules::catalog::radar_position_x_minimum, modules::catalog::radar_position_x_maximum, "%.0f px").display_limits(4.f, 1200.f),
                setting::number("position_y", {"Position Y"}, &model::position_y, modules::catalog::radar_position_y_minimum, modules::catalog::radar_position_y_maximum, "%.0f px").display_limits(4.f, 800.f),
                setting::number("background_opacity", {"Background"}, &model::background_opacity, modules::catalog::radar_background_opacity_minimum, modules::catalog::radar_background_opacity_maximum, "%.2f"),
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
