#pragma once
#include "gui.hpp"
#include <algorithm>
#include <cfloat>
#include <cmath>

namespace gui::watermark_draw
{
    inline ImFont* font()
    {
        return watermark_font ? watermark_font : (ui_bold_font ? ui_bold_font : ImGui::GetFont());
    }
    inline ImVec2 size()
    {
        ImFont* face = font();
        if (!face) return {};
        const float points = 32.f * std::clamp(hud_scale, .5f, 2.f);
        return { face->CalcTextSizeA(points, FLT_MAX, 0, "one").x +
            face->CalcTextSizeA(points, FLT_MAX, 0, "step").x, points };
    }
    inline void draw(ImDrawList* list, float x, float y)
    {
        ImFont* face = font();
        if (!list || !face) return;
        const float scale = std::clamp(hud_scale, .5f, 2.f);
        const float points = 32.f * scale;
        const float one = face->CalcTextSizeA(points, FLT_MAX, 0, "one").x;
        const ImVec2 position(std::round(x), std::round(y));
        const float shadow = std::round(std::max(1.f, 1.5f * scale));
        list->AddText(face, points, ImVec2(position.x + shadow, position.y + shadow), IM_COL32(0, 0, 0, 130), "one");
        list->AddText(face, points, ImVec2(position.x + one + shadow, position.y + shadow), IM_COL32(0, 0, 0, 130), "step");
        list->AddText(face, points, position, IM_COL32(255, 255, 255, 255), "one");
        list->AddText(face, points, ImVec2(position.x + one, position.y), gui::get_dynamic_color(), "step");
    }
}
