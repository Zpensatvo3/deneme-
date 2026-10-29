#pragma once
#ifdef ONESTEP_CMAKE_BUILD
#include <ui/spotify_vector_data.hpp>
#else
#include "spotify_vector_data.hpp"
#endif
#include <imgui/imgui.h>
#include <cmath>

namespace gui
{
    inline void draw_spotify_vector(ImDrawList* draw, ImVec2 minimum, float size)
    {
        if (!draw || !std::isfinite(size) || size <= 0.f) return;
        using namespace spotify_vector_data;
        const ImU32 solid = IM_COL32(red, green, blue, 255), clear = IM_COL32(red, green, blue, 0);
        const ImVec2 uv = ImGui::GetFontTexUvWhitePixel();
        const auto position = [&](float x, float y) { return ImVec2(minimum.x + x * size, minimum.y + y * size); };
        const bool aa = (draw->Flags & ImDrawListFlags_AntiAliasedFill) != 0;
        const int fringe_vertices = aa ? static_cast<int>(outline.size() * 2) : 0;
        draw->PrimReserve(static_cast<int>(triangles.size()) + (aa ? static_cast<int>(outline.size() * 6) : 0),
            static_cast<int>(triangles.size()) + fringe_vertices);
        // Interior triangles share an opaque fill. No antialiased seams between them.
        for (const auto& p : triangles) draw->PrimVtx(position(p.x, p.y), uv, solid);
        if (!aa) return;
        const unsigned base = draw->_VtxCurrentIdx;
        for (const auto& p : outline) {
            const ImVec2 inner = position(p.x, p.y);
            draw->PrimWriteVtx(inner, uv, solid);
            draw->PrimWriteVtx(ImVec2(inner.x + p.nx * draw->_FringeScale, inner.y + p.ny * draw->_FringeScale), uv, clear);
        }
        for (unsigned contour = 0; contour + 1 < contours.size(); ++contour) {
            const unsigned begin = contours[contour], end = contours[contour + 1];
            for (unsigned i = begin; i < end; ++i) {
                const unsigned next = i + 1 == end ? begin : i + 1;
                const unsigned a = base + i * 2, b = base + next * 2;
                for (unsigned index : {a, b, a + 1, a + 1, b, b + 1}) draw->PrimWriteIdx(static_cast<ImDrawIdx>(index));
            }
        }
    }
}
