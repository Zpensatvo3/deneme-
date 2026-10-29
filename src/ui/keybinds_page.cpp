#include "keybinds_page.hpp"
#include "keybinds_model.hpp"
#include "keyboard_layout.hpp"
#include "../configs/keybind_profile.hpp"
#include "../gui/gui.hpp"
#include "../gui/ui_state.hpp"
#include "../gui/control_draw.hpp"
#include "../gui/popup_controls.hpp"
#include <cmath>

namespace {
    struct runtime {
        int selected = 0;
        char search[96]{};
        bool focus_search = false;
        std::string category;
        std::array<float, 104> hover{}, selection{};
        float reveal = 0.f;
    } state;
    float approach(float value, float target) {
        if (gui::ui_state::get_preferences().reduce_motion) return target;
        return value + (target - value) * (1.f - std::exp(-14.f * std::clamp(ImGui::GetIO().DeltaTime, .0001f, .05f)));
    }
    ImFont* regular() { return gui::ui_font ? gui::ui_font : ImGui::GetFont(); }
    ImFont* bold() { return gui::ui_bold_font ? gui::ui_bold_font : ImGui::GetFont(); }
    void text(ImDrawList* draw, ImVec2 p, float size, ImU32 color, const char* value, bool heavy = false) {
        draw->AddText(heavy ? bold() : regular(), gui::typography::readable(size), p, color, value);
    }
    ImU32 tint(float alpha) { auto c = gui::get_dynamic_color().Value; c.w = alpha; return ImGui::GetColorU32(c); }
    std::string title(std::string value) {
        bool next = true;
        for (char& c : value) { if (next) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c))); next = c == ' '; }
        return value;
    }
    void keyboard(float width) {
        const auto layout = gui::keyboard_layout::measure(width);
        const float unit = layout.unit, gap = gui::keyboard_layout::gap(unit);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 6.f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
        ImGui::SetNextWindowContentSize({layout.width, 0});
        if (ImGui::BeginChild("##keyboard_layout", {width, layout.height + (layout.scroll ? 10.f : 0.f)}, ImGuiChildFlags_None,
            ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        const auto top = ImGui::GetCursorScreenPos();
        const auto assigned = gui::keybinds_model::counts();
        auto* draw = ImGui::GetWindowDrawList();
        for (std::size_t i = 0; i < std::size(platform::keys::keyboard); ++i) {
            const auto& key = platform::keys::keyboard[i];
            const ImVec2 minimum(top.x + key.x * unit, top.y + key.y * unit);
            const ImVec2 size(key.w * unit - gap, key.h * unit - gap);
            const ImVec2 maximum(minimum.x + size.x, minimum.y + size.y);
            ImGui::PushID(key.code); ImGui::SetCursorScreenPos(minimum);
            if (ImGui::InvisibleButton("##key", size)) state.selected = key.code;
            const bool hovered = ImGui::IsItemHovered(), selected = state.selected == key.code;
            const bool reserved = platform::keys::reserved_reason(key.code) != nullptr;
            state.hover[i] = approach(state.hover[i], hovered ? 1.f : 0.f);
            state.selection[i] = approach(state.selection[i], selected ? 1.f : 0.f);
            const float highlight = std::max(state.selection[i], state.hover[i] * .4f);
            draw->AddRectFilled(minimum, maximum, ImGui::GetColorU32(ImVec4(.12f, .12f, .15f, 1.f)), 5.f);
            if (assigned[key.code] || highlight > .001f)
                draw->AddRectFilled(minimum, maximum, tint(.08f + .16f * highlight), 4.f);
            draw->AddRect(minimum, maximum, highlight > .01f ? tint(.35f + highlight * .5f)
                : ImGui::GetColorU32(ImVec4(1,1,1,.14f)), 5.f, 0, selected ? 1.5f : 1.f);
            gui::control_draw::centered_label(draw, gui::keyboard_layout::single_character(key) ? bold() : regular(),
                gui::keyboard_layout::font_size(key), minimum, maximum,
                reserved ? IM_COL32(177, 181, 195, 255) : IM_COL32(245, 246, 251, 255), gui::keyboard_layout::legend(key), {2,2});
            if (assigned[key.code]) draw->AddCircleFilled({maximum.x - 4.f, minimum.y + 4.f}, 1.8f, tint(1.f));
            if (hovered) {
                ImGui::BeginTooltip();
              {
                gui::typography::scoped_font tooltip_font(regular());
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 220.f);
                ImGui::TextUnformatted(platform::keys::label(key.code).c_str());
                if (reserved) ImGui::TextDisabled("Reserved key");
                for (const auto* entry : modules::get_modules())
                    if (gui::keybinds_model::visible(entry) && entry->get_keybind() == key.code) ImGui::TextUnformatted(title(entry->get_name()).c_str());
                ImGui::PopTextWrapPos();
              }
                ImGui::EndTooltip();
            }
            ImGui::PopID();
        }
        ImGui::SetCursorScreenPos(top); ImGui::Dummy({layout.width, layout.height});
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }
    void save_feedback() {
        configs::keybind_profile::flush(true);
    }
    void search_field(float width) {
        const auto minimum = ImGui::GetCursorScreenPos();
        const float height = ImGui::GetFrameHeight(), icon_width = 30.f;
        const ImVec2 maximum(minimum.x + width, minimum.y + height);
        auto* draw = ImGui::GetWindowDrawList();
        const bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
            ImGui::IsMouseHoveringRect(minimum, maximum);
        draw->AddRectFilled(minimum, maximum,
            ImGui::GetColorU32(hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg), ImGui::GetStyle().FrameRounding);
        ImGui::BeginGroup();
        // Reserve the icon's space so long queries never run underneath it.
        if (state.focus_search) { ImGui::SetKeyboardFocusHere(); state.focus_search = false; }
        const bool icon_clicked = hovered && ImGui::GetMousePos().x >= maximum.x - icon_width &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0,0,0,0));
        ImGui::SetNextItemWidth(width - icon_width);
        ImGui::InputTextWithHint("##module_search", "Search modules", state.search, IM_ARRAYSIZE(state.search));
        const bool active = ImGui::IsItemActive();
        ImGui::PopStyleColor(); ImGui::PopStyleVar();
        // Defer focus until after the outside-input click has been processed.
        if (icon_clicked) state.focus_search = true;
        const ImU32 color = active ? tint(.9f) : ImGui::GetColorU32(ImGuiCol_TextDisabled);
        const ImVec2 center(maximum.x - icon_width * .5f - 1.f, minimum.y + height * .5f - 1.f);
        draw->AddCircle(center, 4.5f, color, 0, 1.5f);
        draw->AddLine({center.x + 3.f,center.y + 3.f},{center.x + 7.f,center.y + 7.f},color,1.5f);
        if (active) draw->AddRect(minimum, maximum, tint(.45f), ImGui::GetStyle().FrameRounding);
        ImGui::SameLine(0,0); ImGui::Dummy({icon_width,height});
        ImGui::EndGroup();
    }
    void picker(float width) {
        const auto top = ImGui::GetCursorScreenPos();
        auto* draw = ImGui::GetWindowDrawList();
        const auto selected = platform::keys::label(state.selected);
        text(draw, top, 14.f, ImGui::GetColorU32(ImGuiCol_Text), selected.c_str(), true);
        std::string assignments;
        for (const auto* entry : modules::get_modules()) {
            if (!gui::keybinds_model::visible(entry) || entry->get_keybind() != state.selected) continue;
            if (!assignments.empty()) assignments += ", ";
            assignments += title(entry->get_name());
        }
        const float label_width = bold()->CalcTextSizeA(14.f, FLT_MAX, 0, selected.c_str()).x;
        const ImVec4 summary_clip(top.x, top.y, top.x + width, top.y + 22.f);
        draw->AddText(regular(),gui::typography::caption,{top.x + label_width + 14.f,top.y + 1.f},
            ImGui::GetColorU32(ImGuiCol_TextDisabled),assignments.empty() ? "Unassigned" : assignments.c_str(),nullptr,0,&summary_clip);
        const char* reason = platform::keys::reserved_reason(state.selected);
        ImGui::Dummy({width,22});
        if (ImGui::IsItemHovered() && !assignments.empty()) {
            ImGui::BeginTooltip();
            { gui::typography::scoped_font tooltip_font(regular());
              ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+220.f);
              ImGui::TextUnformatted(assignments.c_str()); ImGui::PopTextWrapPos(); }
            ImGui::EndTooltip();
        }
        if (reason) ImGui::TextDisabled("Reserved key - choose another key");
        const bool narrow = width < 340.f;
        const float category_width = narrow ? width : 166.f;
        search_field(narrow ? width : width - 174.f);
        if (!narrow) ImGui::SameLine(0,8);
        ImGui::SetNextItemWidth(category_width);
        const auto category_minimum = ImGui::GetCursorScreenPos();
        const bool category_hovered = ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(category_minimum,
            {category_minimum.x + category_width,category_minimum.y + ImGui::GetFrameHeight()});
        const auto category_background = ImGui::GetStyleColorVec4(category_hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
        // BeginCombo normally gives the arrow a separate button-colored block.
        // Match both pieces even while its popup is open but no longer hovered.
      {
        gui::popup_controls::style category_popup_style;
        ImGui::PushStyleColor(ImGuiCol_Button, category_background);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, category_background);
        const bool category_open = ImGui::BeginCombo("##category", state.category.empty() ? "All categories" : state.category.c_str());
        ImGui::PopStyleColor(2);
        if (category_open) {
            if (gui::popup_controls::choice("##all_categories", "All categories", state.category.empty(), regular(), gui::get_dynamic_color().Value)) state.category.clear();
            for (const auto& category : gui::keybinds_model::categories())
                if (gui::popup_controls::choice(category.c_str(), category.c_str(), state.category == category, regular(), gui::get_dynamic_color().Value)) state.category = category;
            ImGui::EndCombo();
        }
      }
        const auto list = gui::keybinds_model::filtered(state.search, state.category);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
        const float error_height = configs::keybind_profile::healthy() ? 0.f : ImGui::GetTextLineHeightWithSpacing();
        const float list_height = std::max(40.f, ImGui::GetContentRegionAvail().y - error_height);
        if (ImGui::BeginChild("##module_selection", {0,list_height}, ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar)) {
            std::string category;
            for (auto* entry : list) {
                if (category != gui::keybinds_model::category(*entry)) { category = gui::keybinds_model::category(*entry); ImGui::TextDisabled("%s", category.c_str()); }
                ImGui::PushID(entry->get_id());
                const auto p = ImGui::GetCursorScreenPos();
                const float row_width = ImGui::GetContentRegionAvail().x;
                const bool assigned = entry->get_keybind() == state.selected;
                ImGui::BeginDisabled(reason != nullptr && !assigned);
                const bool clicked = ImGui::InvisibleButton("##assign", {std::max(1.f,row_width - 34.f),32});
                auto* row_draw = ImGui::GetWindowDrawList();
                gui::list_row_background(p, {p.x + row_width,p.y + 32}, ImGui::IsItemHovered(), assigned);
                const auto name = title(entry->get_name());
                const ImVec4 clip(p.x + 10,p.y,p.x + std::max(20.f,row_width - 182.f),p.y+32);
                row_draw->AddText(regular(),gui::typography::control,{p.x+10,p.y+8},ImGui::GetColorU32(ImGuiCol_Text),name.c_str(),nullptr,0,&clip);
                const auto binding = platform::keys::label(entry->get_keybind());
                gui::control_draw::centered_label(row_draw,regular(),gui::typography::caption,{p.x + row_width - 182,p.y},{p.x + row_width - 100,p.y+32},
                    ImGui::GetColorU32(ImGuiCol_TextDisabled),binding.c_str(),{3,2});
                gui::control_draw::centered_label(row_draw,bold(),gui::typography::caption,{p.x + row_width - 98,p.y},{p.x+row_width-36,p.y+32},
                    assigned ? tint(1.f) : ImGui::GetColorU32(ImGuiCol_Text),assigned ? "Remove" : "Assign",{2,2});
                if (clicked) {
                    const bool changed = assigned ? gui::keybinds_model::clear(*entry) : gui::keybinds_model::assign(*entry,state.selected);
                    if (changed) save_feedback();
                }
                ImGui::EndDisabled();
                ImGui::SetCursorScreenPos({p.x + row_width - 30.f,p.y + 1.f});
                ImGui::BeginDisabled(entry->get_keybind() == 0);
                if (gui::icon_action("##clear_binding", "\xEF\x80\x8D", "x", "Remove binding")) {
                    if (gui::keybinds_model::clear(*entry)) save_feedback();
                }
                ImGui::EndDisabled();
                ImGui::SetCursorScreenPos(p); ImGui::Dummy({row_width,32});
                ImGui::PopID();
            }
            if (list.empty()) ImGui::TextDisabled("No matching modules.");
        }
        ImGui::EndChild(); ImGui::PopStyleVar();
        if (!configs::keybind_profile::healthy()) ImGui::TextColored({1,.45f,.45f,1}, "%s", configs::keybind_profile::status().c_str());
    }
}

void gui::keybinds_page::reset() { state = {}; }
void gui::keybinds_page::render()
{
    typography::scoped_font workspace_font(regular());
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14,14));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,10.f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,6.f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,ImVec2(8,6));
    ImGui::PushStyleColor(ImGuiCol_ChildBg,ImVec4(.045f,.043f,.055f,.91f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg,ImVec4(.085f,.08f,.10f,.96f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,tint(.15f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,tint(.24f));
    ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(.11f,.10f,.14f,1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,tint(.25f));
    ImGui::PushStyleColor(ImGuiCol_Header,tint(.12f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered,tint(.21f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,tint(.29f));
    if (ImGui::BeginChild("##keybinds_surface",{0,0},ImGuiChildFlags_AlwaysUseWindowPadding,ImGuiWindowFlags_NoScrollbar)) {
        const float width = ImGui::GetContentRegionAvail().x;
        ImGui::TextUnformatted("Keyboard");
        keyboard(width);
        state.reveal = approach(state.reveal,state.selected ? 1.f : 0.f);
        if (state.selected) {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha,ImGui::GetStyle().Alpha * std::max(.05f,state.reveal));
            picker(width); ImGui::PopStyleVar();
        } else {
            ImGui::Spacing(); ImGui::TextDisabled("Select a key to manage its module bindings.");
            ImGui::PushTextWrapPos(0.f);
            ImGui::TextDisabled("Colored keys have assignments. Esc, Right Shift, End and Windows keys are reserved.");
            ImGui::PopTextWrapPos();
        }
    }
    ImGui::EndChild(); ImGui::PopStyleColor(9); ImGui::PopStyleVar(4);
}
