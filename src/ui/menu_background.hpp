#pragma once

#include <imgui/imgui.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace gui::background
{
    // Stored in local UI preferences, never in shared gameplay configs.
    enum class style : int { classic, citadel, candy_dusk, nebula, aurora, custom, count };

    struct options
    {
        style selected = style::classic;
        float opacity = .85f;
        float dimming = .48f;
    };

    inline void normalize(options& value)
    {
        if (static_cast<int>(value.selected) < 0 || value.selected >= style::count)
            value.selected = style::classic;
        value.opacity = std::isfinite(value.opacity) ? std::clamp(value.opacity, 0.f, 1.f) : .85f;
        value.dimming = std::isfinite(value.dimming) ? std::clamp(value.dimming, .25f, .85f) : .48f;
    }

    inline constexpr std::size_t max_file_bytes = 16u * 1024u * 1024u;
    inline constexpr bool valid_dimensions(std::uint32_t width, std::uint32_t height)
    {
        return width > 0 && height > 0 && width <= 8192 && height <= 8192 &&
            static_cast<std::uint64_t>(width) * height <= 16u * 1024u * 1024u;
    }

    struct uv_rect { ImVec2 minimum{0, 0}, maximum{1, 1}; };
    inline uv_rect cover_uv(ImVec2 image, ImVec2 target)
    {
        if (!std::isfinite(image.x) || !std::isfinite(image.y) || !std::isfinite(target.x) ||
            !std::isfinite(target.y) || image.x <= 0 || image.y <= 0 || target.x <= 0 || target.y <= 0)
            return {};
        const float source_ratio = image.x / image.y, target_ratio = target.x / target.y;
        if (source_ratio > target_ratio)
        {
            const float half = .5f * target_ratio / source_ratio;
            return {{.5f - half, 0}, {.5f + half, 1}};
        }
        const float half = .5f * source_ratio / target_ratio;
        return {{0, .5f - half}, {1, .5f + half}};
    }

    struct texture_view { ImTextureID id = nullptr; ImVec2 size{}; };
    enum class import_state { idle, choosing, imported, failed, save_failed };

    // Platform functions run on the UI OpenGL context, except the file picker.
    texture_view texture(style selected);
    void begin_import();
    import_state status();
    void update();
    void shutdown();
    bool render(ImDrawList* draw, ImVec2 minimum, ImVec2 size, const options& value);
    void render_settings(ImU32 accent);
}
