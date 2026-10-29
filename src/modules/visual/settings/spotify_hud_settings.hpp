#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct spotify_hud
    {
        float position_x = 22.0f;
        float position_y = 60.0f;
        float scale = 1.2f;
        float background_opacity = 0.72f;
        bool show_artwork = true;
        bool show_progress = true;
        bool show_visualizer = true;
        bool show_brand = true;
        bool show_lyrics = true;
        int lyric_lines = 1;
        bool enable_lyrics = true;
        bool show_when_idle = true;
        bool operator==(const spotify_hud&) const = default;
        static std::span<const modules::settings::descriptor<spotify_hud>> schema()
        {
            using model = spotify_hud;
            using setting = modules::settings::descriptor<model>;
            static const setting fields[]{
                setting::number("position_x", {"Position X"}, &model::position_x, 0.0f, 1000000, "%.0f px"),
                setting::number("position_y", {"Position Y"}, &model::position_y, 0.0f, 1000000, "%.0f px"),
                setting::number("background_opacity", {"Background"}, &model::background_opacity, 0.20f, 1.0f, "%.2f"),
                setting::toggle("show_when_idle", {"Show When Idle"}, &model::show_when_idle),
                setting::toggle("show_artwork", {"Album Art"}, &model::show_artwork),
                setting::toggle("show_progress", {"Progress Bar"}, &model::show_progress),
                setting::toggle("show_visualizer", {"Visualizer"}, &model::show_visualizer),
                setting::toggle("show_brand", {"Spotify Logo"}, &model::show_brand),
                setting::toggle("show_lyrics", {"Lyrics"}, &model::show_lyrics),
                setting::toggle("enable_lyrics", {"Fetch Lyrics Online"}, &model::enable_lyrics).when([](const model& c) { return c.show_lyrics; }),
                setting::number("scale", {"scale"}, &model::scale, 1.2f, 1.2f, "%.2f").hidden(),
                setting::integer("lyric_lines", {"lyric_lines"}, &model::lyric_lines, 1, 1, "%d").hidden(),
            };
            return fields;
        }
    };
}
