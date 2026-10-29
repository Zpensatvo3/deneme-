#pragma once
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <algorithm>
#include <cfloat>
#include <cmath>

namespace gui::control_draw
{
    // Line height includes ascent/descent padding. Small badges need the visible
    // glyph bounds instead, especially lowercase labels such as "none".
    inline ImVec4 text_ink_bounds(ImFont* font, float size, const char* text)
    {
        const float scale = size / font->FontSize;
        float cursor = 0;
        ImVec4 bounds(FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX);
        while (text && *text && *text != '\n') {
            unsigned int codepoint = 0;
            const int consumed = ImTextCharFromUtf8(&codepoint, text, nullptr);
            if (consumed <= 0) break;
            text += consumed;
            const auto* glyph = font->FindGlyph(static_cast<ImWchar>(codepoint));
            if (!glyph) continue;
            if (glyph->Visible) {
                bounds.x = std::min(bounds.x, cursor + glyph->X0 * scale);
                bounds.y = std::min(bounds.y, glyph->Y0 * scale);
                bounds.z = std::max(bounds.z, cursor + glyph->X1 * scale);
                bounds.w = std::max(bounds.w, glyph->Y1 * scale);
            }
            cursor += glyph->AdvanceX * scale;
        }
        return bounds.x == FLT_MAX ? ImVec4(0, 0, cursor, size) : bounds;
    }

    inline void centered_label(ImDrawList* draw, ImFont* font, float size, ImVec2 minimum,
        ImVec2 maximum, ImU32 color, const char* text, ImVec2 padding = ImVec2(8, 3))
    {
        ImVec4 bounds = text_ink_bounds(font, size, text);
        const float width = std::max(1.f, maximum.x - minimum.x - 2 * padding.x);
        const float height = std::max(1.f, maximum.y - minimum.y - 2 * padding.y);
        const float fit = std::min({1.f, width / std::max(1.f, bounds.z - bounds.x), height / std::max(1.f, bounds.w - bounds.y)});
        size *= fit;
        bounds = text_ink_bounds(font, size, text);
        const ImVec2 position(std::round((minimum.x + maximum.x - bounds.x - bounds.z) * .5f),
            std::round((minimum.y + maximum.y - bounds.y - bounds.w) * .5f));
        const ImVec4 clip(minimum.x + padding.x, minimum.y + padding.y,
            maximum.x - padding.x, maximum.y - padding.y);
        draw->AddText(font, size, position, color, text, nullptr, 0, &clip);
    }

    inline void close_icon(ImDrawList* draw, ImVec2 center, ImU32 color)
    {
        center.x = std::floor(center.x) + .5f;
        center.y = std::floor(center.y) + .5f;
        constexpr float arm = 5.8f, half_stroke = .85f, rotation = .70710678f;
        const ImVec2 outline[] = {
            {-half_stroke, -arm}, {half_stroke, -arm}, {half_stroke, -half_stroke},
            {arm, -half_stroke}, {arm, half_stroke}, {half_stroke, half_stroke},
            {half_stroke, arm}, {-half_stroke, arm}, {-half_stroke, half_stroke},
            {-arm, half_stroke}, {-arm, -half_stroke}, {-half_stroke, -half_stroke}
        };
        ImVec2 points[12];
        for (int i = 0; i < 12; ++i)
            points[i] = ImVec2(center.x + (outline[i].x - outline[i].y) * rotation,
                center.y + (outline[i].x + outline[i].y) * rotation);
        // One AA silhouette avoids double blending where two diagonal lines meet.
        draw->AddConcavePolyFilled(points, 12, color);
    }
}
