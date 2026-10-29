#pragma once
#include <imgui/imgui.h>

namespace gui::surface_style
{
    inline void configure(ImGuiStyle& style)
    {
        style.AntiAliasedLines = true;
        style.AntiAliasedLinesUseTex = true;
        style.AntiAliasedFill = true;
        style.ScrollbarSize = 0.f;
        style.CurveTessellationTol = .15f;
        style.CircleTessellationMaxError = .08f;
        style.PopupRounding = 12.f;
        style.PopupBorderSize = 1.f;
    }

}
