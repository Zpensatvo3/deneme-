#pragma once
#include "ui_typography.hpp"
#include "control_draw.hpp"
#include <string>

namespace gui::popup_controls
{
    inline ImU32 tint(ImVec4 color, float opacity)
    {
        color.w *= opacity;
        return ImGui::GetColorU32(color);
    }
    class style
    {
    public:
        style()
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(typography::popup_padding, typography::popup_padding));
            ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, typography::popup_rounding);
            ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.f);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, typography::popup_gap));
            // The workspace hides scrollbars globally. A constrained native
            // combo still needs a nonzero track when its contents overflow.
            ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 6.f);
            ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(.065f, .063f, .080f, .99f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.f, 1.f, 1.f, .10f));
            // Rows paint their own inset rounded selection and hover surface.
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0, 0, 0, 0));
        }
        ~style() { ImGui::PopStyleColor(5); ImGui::PopStyleVar(5); }
        style(const style&) = delete;
        style& operator=(const style&) = delete;
    };

    inline bool choice(const char* id, const char* text, bool selected, ImFont* font, ImVec4 accent)
    {
        const auto minimum = ImGui::GetCursorScreenPos();
        const float width = std::max(1.f, ImGui::GetContentRegionAvail().x);
        const ImVec2 maximum(minimum.x + width, minimum.y + typography::choice_height);
        const bool clicked = ImGui::Selectable(id, selected, ImGuiSelectableFlags_NoPadWithHalfSpacing,
            ImVec2(width, typography::choice_height));
        const bool hovered = ImGui::IsItemHovered() || ImGui::IsItemFocused();
        auto* draw = ImGui::GetWindowDrawList();
        if (hovered || selected)
            draw->AddRectFilled(minimum, maximum,
                selected ? tint(accent, hovered ? .23f : .14f) : ImGui::GetColorU32(ImVec4(1, 1, 1, .055f)),
                typography::row_rounding);
        constexpr float size = typography::control;
        const float available = std::max(1.f, width - 42.f);
        std::string display = text ? text : "";
        if (font->CalcTextSizeA(size, FLT_MAX, 0.f, display.c_str()).x > available) {
            const float dots = font->CalcTextSizeA(size, FLT_MAX, 0.f, "...").x;
            const char* end = display.c_str();
            font->CalcTextSizeA(size, std::max(0.f, available - dots), 0.f, display.c_str(), nullptr, &end);
            display = std::string(display.c_str(), end) + "...";
        }
        const auto ink = control_draw::text_ink_bounds(font, size, display.c_str());
        const ImVec4 clip(minimum.x + 10.f, minimum.y, maximum.x - 26.f, maximum.y);
        draw->AddText(font, size, ImVec2(minimum.x + 10.f, std::round((minimum.y + maximum.y - ink.y - ink.w) * .5f)),
            selected ? tint(accent, 1.f) : ImGui::GetColorU32(ImGuiCol_Text), display.c_str(), nullptr, 0, &clip);
        if (selected) {
            const ImVec2 center(maximum.x - 13.f, minimum.y + typography::choice_height * .5f);
            const ImVec2 check[] = {{center.x - 4.f, center.y}, {center.x - 1.f, center.y + 3.f}, {center.x + 4.f, center.y - 3.f}};
            draw->AddPolyline(check, 3, tint(accent, 1.f), 0, 1.6f);
            ImGui::SetItemDefaultFocus();
        }
        if (hovered && display != text) ImGui::SetTooltip("%s", text ? text : "");
        return clicked;
    }
}
