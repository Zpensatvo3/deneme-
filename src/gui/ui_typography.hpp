#pragma once
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <algorithm>

namespace gui::typography
{
    // Workspace controls only; HUD sizes and user-selected HUD scale stay intact.
    inline constexpr float caption = 14.f, control = 15.f, title = 19.f;
    inline constexpr float choice_height = 36.f, popup_padding = 8.f, popup_gap = 4.f;
    inline constexpr float popup_rounding = 12.f, row_rounding = 7.f;
    inline float readable(float size) { return std::max(caption, size); }

    class scoped_font
    {
        ImGuiWindow* window_;
        float previous_scale_;
    public:
        explicit scoped_font(ImFont* font, float size = control)
            : window_(ImGui::GetCurrentWindow()), previous_scale_(window_->FontWindowScale)
        {
            ImGui::PushFont(font ? font : ImGui::GetFont());
            ImGui::SetWindowFontScale(previous_scale_ * size / ImGui::GetFontSize());
        }
        ~scoped_font()
        {
            IM_ASSERT(ImGui::GetCurrentWindow() == window_);
            ImGui::SetWindowFontScale(previous_scale_);
            ImGui::PopFont();
        }
        scoped_font(const scoped_font&) = delete;
        scoped_font& operator=(const scoped_font&) = delete;
    };
}
