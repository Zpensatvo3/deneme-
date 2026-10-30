#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct spotify_hud
    {
        float position_x = modules::catalog::spotify_hud_position_x_default;
        float position_y = modules::catalog::spotify_hud_position_y_default;
        float scale = modules::catalog::spotify_hud_scale_default;
        float background_opacity = modules::catalog::spotify_hud_background_opacity_default;
        bool show_artwork = modules::catalog::spotify_hud_show_artwork_default;
        bool show_progress = modules::catalog::spotify_hud_show_progress_default;
        bool show_visualizer = modules::catalog::spotify_hud_show_visualizer_default;
        bool show_brand = modules::catalog::spotify_hud_show_brand_default;
        bool show_lyrics = modules::catalog::spotify_hud_show_lyrics_default;
        int lyric_lines = modules::catalog::spotify_hud_lyric_lines_default;
        bool enable_lyrics = modules::catalog::spotify_hud_enable_lyrics_default;
        bool show_when_idle = modules::catalog::spotify_hud_show_when_idle_default;
        bool operator==(const spotify_hud&) const = default;
        static std::span<const modules::settings::descriptor<spotify_hud>> schema()
        {
            using model = spotify_hud;
            using setting = modules::settings::descriptor<model>;
            static const setting fields[]{
                setting::number("position_x", {"Position X"}, &model::position_x, modules::catalog::spotify_hud_position_x_minimum, modules::catalog::spotify_hud_position_x_maximum, "%.0f px"),
                setting::number("position_y", {"Position Y"}, &model::position_y, modules::catalog::spotify_hud_position_y_minimum, modules::catalog::spotify_hud_position_y_maximum, "%.0f px"),
                setting::number("background_opacity", {"Background"}, &model::background_opacity, modules::catalog::spotify_hud_background_opacity_minimum, modules::catalog::spotify_hud_background_opacity_maximum, "%.2f"),
                setting::toggle("show_when_idle", {"Show When Idle"}, &model::show_when_idle),
                setting::toggle("show_artwork", {"Album Art"}, &model::show_artwork),
                setting::toggle("show_progress", {"Progress Bar"}, &model::show_progress),
                setting::toggle("show_visualizer", {"Visualizer"}, &model::show_visualizer),
                setting::toggle("show_brand", {"Spotify Logo"}, &model::show_brand),
                setting::toggle("show_lyrics", {"Lyrics"}, &model::show_lyrics),
                setting::toggle("enable_lyrics", {"Fetch Lyrics Online"}, &model::enable_lyrics).when([](const model& c) { return c.show_lyrics; }),
                setting::number("scale", {"scale"}, &model::scale, modules::catalog::spotify_hud_scale_minimum, modules::catalog::spotify_hud_scale_maximum, "%.2f").hidden(),
                setting::integer("lyric_lines", {"lyric_lines"}, &model::lyric_lines, modules::catalog::spotify_hud_lyric_lines_minimum, modules::catalog::spotify_hud_lyric_lines_maximum, "%d").hidden(),
            };
            return fields;
        }
    };
}
