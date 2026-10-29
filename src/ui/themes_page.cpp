#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "themes_page.hpp"
#include "menu_background.hpp"
#include "../gui/gui.hpp"
#include "../gui/ui_state.hpp"
#include "../gui/theme_palette.hpp"
#include "../gui/ui_typography.hpp"
#include <algorithm>

namespace gui::themes_page
{
    namespace
    {
        ImU32 tone(theme::color color, float opacity = 1.f)
        {
            return ImGui::GetColorU32(ImVec4(color.r, color.g, color.b, opacity));
        }
        void render_palettes()
        {
            auto* draw = ImGui::GetWindowDrawList();
            auto* text_font = gui::ui_font ? gui::ui_font : ImGui::GetFont();
            auto* bold = gui::ui_bold_font ? gui::ui_bold_font : text_font;
            const auto heading = ImGui::GetCursorScreenPos();
            const float width = ImGui::GetContentRegionAvail().x;
            draw->AddText(bold, typography::control, heading, IM_COL32(234, 234, 241, 255), "Accent color");
            ImGui::Dummy(ImVec2(0, 24));

            constexpr float gap = 8.f;
            const int columns = std::clamp(static_cast<int>((width + gap) / 174.f), 1, 4);
            const ImVec2 size(std::max(1.f, (width - gap * (columns - 1)) / columns), 38.f);
            const auto start = ImGui::GetCursorScreenPos();
            for (int i = 0; i < theme::count; ++i) {
                ImGui::PushID(i);
                const auto p = ImVec2(start.x + (i % columns) * (size.x + gap), start.y + (i / columns) * (size.y + gap));
                const auto maximum = ImVec2(p.x + size.x, p.y + size.y);
                ImGui::SetCursorScreenPos(p);
                if (ImGui::InvisibleButton("##palette", size) && gui::active_theme != i) {
                    gui::active_theme = i;
                    ui_state::mark_preferences_dirty();
                }
                const bool selected = gui::active_theme == i, hovered = ImGui::IsItemHovered();
                const auto& palette = theme::palettes[i];
                draw->AddRectFilled(p, maximum, selected ? tone(palette.stops[0], .10f)
                    : IM_COL32(255, 255, 255, hovered ? 12 : 5), 6.f);
                draw->AddRect(p, maximum, selected ? tone(palette.stops[0], .65f)
                    : IM_COL32(255, 255, 255, hovered ? 30 : 13), 6.f);
                for (int stop = 0; stop < 3; ++stop)
                    draw->AddCircleFilled(ImVec2(p.x + 13 + stop * 7.f, p.y + 19), 4.5f, tone(palette.stops[stop]));
                const ImVec4 clip(p.x + 40, p.y, maximum.x - 20, maximum.y);
                draw->AddText(text_font, typography::caption, ImVec2(p.x + 40, p.y + 11), IM_COL32(234, 234, 241, 255),
                    i == 0 ? "OneStep" : palette.name, nullptr, 0.f, &clip);
                if (selected) {
                    const ImVec2 check(maximum.x - 13, p.y + 19);
                    const ImVec2 points[] = { {check.x - 3, check.y}, {check.x - 1, check.y + 2}, {check.x + 3, check.y - 3} };
                    draw->AddPolyline(points, 3, tone(palette.stops[0]), 0, 1.4f);
                }
                ImGui::PopID();
            }
            const int rows = (theme::count + columns - 1) / columns;
            ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + rows * (size.y + gap) - gap));
            ImGui::Dummy(ImVec2(0, 8));
        }
    }

    void render()
    {
        typography::scoped_font workspace_font(gui::ui_font);
        ImGui::PushID("theme_selection");
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f);
        ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 6.f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18, 18));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 6));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(.045f, .043f, .055f, .91f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1, 1, 1, .055f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(.105f, .10f, .13f, .9f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(.14f, .13f, .18f, .95f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(.16f, .14f, .21f, .95f));
        const auto accent = gui::get_dynamic_color();
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, accent.Value);
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, accent.Value);
        if (ImGui::BeginChild("##appearance_surface", ImVec2(0, 0), ImGuiChildFlags_Border, ImGuiWindowFlags_NoScrollbar)) {
            render_palettes();
            gui::background::render_settings(gui::get_dynamic_color());
        }
        ImGui::EndChild();
        ImGui::PopStyleColor(7);
        ImGui::PopStyleVar(6);
        ImGui::PopID();
    }
}
