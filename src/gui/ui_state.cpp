#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "ui_state.hpp"
#include "gui.hpp"
#include "theme_palette.hpp"

#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <string>

namespace
{
    gui::ui_state::preferences state{};
    bool loaded = false;
    struct save_schedule
    {
        using clock = std::chrono::steady_clock;
        bool dirty = false;
        clock::time_point changed_at{}, retry_after{};

        void changed(clock::time_point now) { dirty = true; changed_at = now; }
        bool ready(clock::time_point now, bool force, bool interacting) const
        {
            return dirty && (force || (!interacting && now >= retry_after &&
                now - changed_at >= std::chrono::milliseconds(650)));
        }
        void attempted(clock::time_point now) { retry_after = now + std::chrono::seconds(2); }
        void saved() { dirty = false; retry_after = {}; }
    } pending_save;

    std::filesystem::path preferences_path()
    {
        wchar_t local_app_data[32768]{};
        const DWORD length = GetEnvironmentVariableW(
            L"LOCALAPPDATA", local_app_data, static_cast<DWORD>(std::size(local_app_data)));
        if (length == 0 || length >= std::size(local_app_data)) return {};
        std::filesystem::path dir = std::filesystem::path(local_app_data) / L"onestep";
        std::error_code ec;
        if (!std::filesystem::exists(dir, ec))
        {
            std::filesystem::path old_dir = std::filesystem::path(local_app_data) / L"Thedorius Lite";
            if (std::filesystem::exists(old_dir, ec))
                return old_dir / L"ui-preferences.json";
        }
        return dir / L"ui-preferences.json";
    }

    bool read_number(const std::string& source, const char* key, float& output)
    {
        const std::string token = std::string("\"") + key + "\"";
        const std::size_t key_position = source.find(token);
        if (key_position == std::string::npos) return false;
        const std::size_t colon = source.find(':', key_position + token.size());
        if (colon == std::string::npos) return false;
        try
        {
            std::size_t consumed = 0;
            const std::string tail = source.substr(colon + 1);
            const float value = std::stof(tail, &consumed);
            if (!std::isfinite(value) || consumed == 0) return false;
            const std::size_t end = tail.find_first_not_of(" \t\r\n", consumed);
            if (end != std::string::npos && tail[end] != ',' && tail[end] != '}') return false;
            output = value;
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool read_integer(const std::string& source, const char* key, int& output)
    {
        float value = 0.0f;
        if (!read_number(source, key, value)) return false;
        if (std::trunc(value) != value || static_cast<double>(value) < std::numeric_limits<int>::min() ||
            static_cast<double>(value) > std::numeric_limits<int>::max()) return false;
        output = static_cast<int>(value);
        return true;
    }

    bool read_boolean(const std::string& source, const char* key, bool& output)
    {
        const std::string token = std::string("\"") + key + "\"";
        const std::size_t key_position = source.find(token);
        if (key_position == std::string::npos) return false;
        const std::size_t colon = source.find(':', key_position + token.size());
        if (colon == std::string::npos) return false;
        const std::size_t value = source.find_first_not_of(" \t\r\n", colon + 1);
        if (value == std::string::npos) return false;
        if (source.compare(value, 4, "true") == 0) { output = true; return true; }
        if (source.compare(value, 5, "false") == 0) { output = false; return true; }
        return false;
    }

    void write_hud_preferences(std::ostream& output, const gui::ui_state::preferences& value)
    {
        output << "{\n  \"has_hud_layout\": " << (value.has_hud_layout ? "true" : "false") << ",\n";
        const char* ids[] = { "watermark", "arraylist", "spotify" };
        for (std::size_t index = 0; index < value.hud_layout.size(); ++index)
        {
            const auto& item = value.hud_layout[index];
            const std::string prefix = std::string("hud_") + ids[index];
            output << "  \"" << prefix << "_x\": " << item.x << ",\n"
                << "  \"" << prefix << "_y\": " << item.y << ",\n"
                << "  \"" << prefix << "_scale\": " << item.scale << ",\n"
                << "  \"" << prefix << "_visible\": " << (item.visible ? "true" : "false") << ",\n";
        }
    }

    void normalize(gui::ui_state::preferences& value)
    {
        gui::background::normalize(value.menu_background);
        if (!std::isfinite(value.window_position.x)) value.window_position.x = 0.0f;
        if (!std::isfinite(value.window_position.y)) value.window_position.y = 0.0f;
        if (!std::isfinite(value.window_size.x)) value.window_size.x = 1280.0f;
        if (!std::isfinite(value.window_size.y)) value.window_size.y = 620.0f;
        value.version = 1;
        value.layout = static_cast<gui::ui_state::layout_mode>(
            std::clamp(static_cast<int>(value.layout), 0, 1));
        value.energy = static_cast<gui::ui_state::energy_quality>(
            std::clamp(static_cast<int>(value.energy), 0, 2));
        value.window_size.x = std::clamp(value.window_size.x, 980.0f, 1800.0f);
        value.window_size.y = std::clamp(value.window_size.y, 520.0f, 1100.0f);
        for (auto& item : value.hud_layout)
        {
            item.x = std::isfinite(item.x) ? std::clamp(item.x, 0.f, 32768.f) : 10.f;
            item.y = std::isfinite(item.y) ? std::clamp(item.y, 0.f, 32768.f) : 10.f;
            item.scale = std::isfinite(item.scale) ? std::clamp(item.scale, .5f, 2.f) : 1.f;
        }
        value.hud_layout[2].scale = 1.2f;
    }
}

gui::ui_state::preferences& gui::ui_state::get_preferences()
{
    if (!loaded) load_preferences();
    return state;
}

void gui::ui_state::load_preferences()
{
    if (loaded) return;
    loaded = true;
    state = {};

    const std::filesystem::path path = preferences_path();
    if (path.empty()) return;
    std::ifstream input(path, std::ios::binary);
    if (!input) return;
    const std::string source((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());

    int version = 0;
    int layout = 0;
    int energy = 1;
    float x = 0.0f, y = 0.0f, width = 1280.0f, height = 620.0f;
    bool has_geometry = false;
    bool reduce_motion = false;
    if (!read_integer(source, "version", version) || version != 1) return;
    read_integer(source, "layout", layout);
    read_integer(source, "energy", energy);
    read_boolean(source, "reduce_motion", reduce_motion);
    read_boolean(source, "has_geometry", has_geometry);
    read_number(source, "window_x", x);
    read_number(source, "window_y", y);
    read_number(source, "window_width", width);
    read_number(source, "window_height", height);

    int theme = 0;
    if (read_integer(source, "theme", theme))
        gui::active_theme = std::clamp(theme, 0, gui::theme::count - 1);

    state.layout = static_cast<layout_mode>(layout);
    state.energy = static_cast<energy_quality>(energy);
    state.reduce_motion = reduce_motion;
    int background_style = 0;
    read_integer(source, "menu_background", background_style);
    state.menu_background.selected = static_cast<gui::background::style>(background_style);
    read_number(source, "menu_background_opacity", state.menu_background.opacity);
    read_number(source, "menu_background_dimming", state.menu_background.dimming);
    state.has_geometry = has_geometry;
    state.window_position = ImVec2(x, y);
    state.window_size = ImVec2(width, height);
    read_boolean(source, "has_hud_layout", state.has_hud_layout);
    const char* ids[] = { "watermark", "arraylist", "spotify" };
    for (std::size_t index = 0; index < state.hud_layout.size(); ++index)
    {
        auto& item = state.hud_layout[index];
        const std::string prefix = std::string("hud_") + ids[index];
        read_number(source, (prefix + "_x").c_str(), item.x);
        read_number(source, (prefix + "_y").c_str(), item.y);
        read_number(source, (prefix + "_scale").c_str(), item.scale);
        read_boolean(source, (prefix + "_visible").c_str(), item.visible);
    }
    normalize(state);
}

void gui::ui_state::mark_preferences_dirty()
{
    pending_save.changed(std::chrono::steady_clock::now());
}

void gui::ui_state::flush_preferences(bool force, bool interacting)
{
    const auto now = std::chrono::steady_clock::now();
    if (!pending_save.ready(now, force, interacting)) return;
    // Failure must not turn a frame callback into an unbounded disk retry loop.
    pending_save.attempted(now);

    normalize(state);
    const std::filesystem::path path = preferences_path();
    gui::active_theme = std::clamp(gui::active_theme, 0, gui::theme::count - 1);
    if (path.empty()) return;
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) return;

    const std::filesystem::path temporary = path.wstring() + L".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) return;
        write_hud_preferences(output, state);
        output << "  \"version\": 1,\n"
            << "  \"layout\": " << static_cast<int>(state.layout) << ",\n"
            << "  \"energy\": " << static_cast<int>(state.energy) << ",\n"
            << "  \"theme\": " << gui::active_theme << ",\n"
            << "  \"reduce_motion\": " << (state.reduce_motion ? "true" : "false") << ",\n"
            << "  \"menu_background\": " << static_cast<int>(state.menu_background.selected) << ",\n"
            << "  \"menu_background_opacity\": " << state.menu_background.opacity << ",\n"
            << "  \"menu_background_dimming\": " << state.menu_background.dimming << ",\n"
            << "  \"has_geometry\": " << (state.has_geometry ? "true" : "false") << ",\n"
            << std::fixed << std::setprecision(1)
            << "  \"window_x\": " << state.window_position.x << ",\n"
            << "  \"window_y\": " << state.window_position.y << ",\n"
            << "  \"window_width\": " << state.window_size.x << ",\n"
            << "  \"window_height\": " << state.window_size.y << "\n"
            << "}\n";
        if (!output) return;
    }

    if (MoveFileExW(temporary.c_str(), path.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        pending_save.saved();
    }
    else
    {
        std::filesystem::remove(temporary, error);
    }
}

void gui::ui_state::reset_preferences()
{
    state = {};
    gui::active_theme = 0;
    loaded = true;
    mark_preferences_dirty();
    flush_preferences(true);
}
