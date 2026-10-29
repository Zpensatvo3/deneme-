#pragma once
#include "../gui/ui_typography.hpp"
#include "../gui/control_draw.hpp"
#include <algorithm>
#include <cctype>
#include <span>
#include <string>
#include <vector>

namespace gui::block_picker
{
    struct entry { std::string key, name, searchable; };
    inline std::string lowercase(std::string text)
    {
        for (char& character : text) character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        return text;
    }
    struct state { bool selected_only = false, search_open = false; char search[64]{}; };

    template<class DrawIcon> bool render(std::span<const entry> catalog, std::vector<std::string>& selected,
        state& view, ImFont* font, ImVec4 accent, DrawIcon draw_icon)
    {
        typography::scoped_font readable(font);
        bool changed = false;
        const float width = ImGui::GetContentRegionAvail().x;
        ImGui::TextUnformatted("Blocks"); ImGui::SameLine();
        ImGui::TextDisabled("%zu selected", selected.size());
        constexpr float gap = 4.f, tool = 30.f;
        const float tab_width = std::max(1.f, (width - 2.f * tool - 3.f * gap) * .5f);
        const auto tab = [&](const char* label, bool active, ImVec2 size) {
            if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(accent.x, accent.y, accent.z, .26f));
            const bool clicked = ImGui::Button(label, size);
            if (active) ImGui::PopStyleColor();
            return clicked;
        };
        if (tab("All", !view.selected_only, {tab_width, tool})) view.selected_only = false;
        ImGui::SameLine(0,gap);
        if (tab("Selected", view.selected_only, {tab_width, tool})) view.selected_only = true;
        ImGui::SameLine(0,gap);
        const auto search_min = ImGui::GetCursorScreenPos();
        const bool open_search = tab("##filter_blocks", view.search_open, {tool,tool});
        const ImVec2 center(search_min.x+13.f,search_min.y+13.f);
        auto* draw = ImGui::GetWindowDrawList();
        draw->AddCircle(center,4.5f,ImGui::GetColorU32(ImGuiCol_Text),12,1.5f);
        draw->AddLine({center.x+3.5f,center.y+3.5f},{center.x+7.f,center.y+7.f},ImGui::GetColorU32(ImGuiCol_Text),1.5f);
        if (open_search) { view.search_open = !view.search_open; if (!view.search_open) view.search[0] = 0; }
        ImGui::SameLine(0,gap);
        const auto clear_min = ImGui::GetCursorScreenPos();
        if (ImGui::Button("##clear_blocks", {tool,tool}) && !selected.empty()) { selected.clear(); changed = true; }
        control_draw::close_icon(draw,{clear_min.x+tool*.5f,clear_min.y+tool*.5f},ImGui::GetColorU32(ImGuiCol_Text));
        if (view.search_open)
        {
            ImGui::SetNextItemWidth(width);
            if (open_search) ImGui::SetKeyboardFocusHere();
            ImGui::InputTextWithHint("##block_filter", "Filter", view.search, IM_ARRAYSIZE(view.search));
        }
        const auto query = lowercase(view.search);
        std::vector<const entry*> filtered;
        for (const auto& item : catalog)
            if ((!view.selected_only || std::find(selected.begin(),selected.end(),item.key) != selected.end())
                && (query.empty() || item.searchable.find(query) != std::string::npos)) filtered.push_back(&item);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {4.f,4.f});
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {gap,gap});
        if (ImGui::BeginChild("##block_catalog", {0,224.f}, ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollbar))
        {
            const float available = ImGui::GetContentRegionAvail().x;
            const int columns = std::max(1, static_cast<int>((available+gap)/48.f));
            const float tile = std::max(1.f,(available-gap*(columns-1))/columns);
            const float pitch = tile+gap;
            const int rows = static_cast<int>((filtered.size()+columns-1)/columns);
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            ImGuiListClipper clipper;
            clipper.Begin(rows,pitch);
            while (clipper.Step()) for (int row=clipper.DisplayStart; row<clipper.DisplayEnd; ++row)
            {
                for (int column=0; column<columns; ++column)
                {
                    const auto index = static_cast<std::size_t>(row*columns+column);
                    if (index >= filtered.size()) break;
                    const auto& item = *filtered[index];
                    const ImVec2 minimum(origin.x+column*pitch,origin.y+row*pitch), maximum(minimum.x+tile,minimum.y+tile);
                    ImGui::SetCursorScreenPos(minimum); ImGui::PushID(item.key.c_str());
                    const bool clicked = ImGui::InvisibleButton("##block",{tile,tile});
                    const bool hovered = ImGui::IsItemHovered();
                    auto found = std::find(selected.begin(),selected.end(),item.key);
                    if (clicked) { if (found==selected.end()) selected.push_back(item.key); else selected.erase(found); changed=true; }
                    const bool active = std::find(selected.begin(),selected.end(),item.key)!=selected.end();
                    auto* list = ImGui::GetWindowDrawList();
                    list->AddRectFilled(minimum,maximum,ImGui::GetColorU32(active ? ImVec4(accent.x,accent.y,accent.z,.2f)
                        : ImVec4(1,1,1,hovered?.09f:.035f)),7.f);
                    if (active || hovered) list->AddRect(minimum,maximum,ImGui::GetColorU32(active ? accent : ImVec4(1,1,1,.2f)),7.f);
                    const float inset = std::max(5.f,(tile-34.f)*.5f);
                    draw_icon(item.key,list,{minimum.x+inset,minimum.y+inset},{maximum.x-inset,maximum.y-inset});
                    if (active)
                    {
                        const ImVec2 badge(maximum.x-7.f,minimum.y+7.f);
                        list->AddCircleFilled(badge,4.f,ImGui::GetColorU32(accent));
                        list->AddLine({badge.x-2.f,badge.y},{badge.x-.5f,badge.y+1.5f},IM_COL32_WHITE,1.f);
                        list->AddLine({badge.x-.5f,badge.y+1.5f},{badge.x+2.f,badge.y-1.5f},IM_COL32_WHITE,1.f);
                    }
                    if (hovered)
                    {
                        ImGui::BeginTooltip();
                        { typography::scoped_font tooltip_font(font); ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+220.f);
                          ImGui::TextUnformatted(item.name.c_str()); ImGui::PopTextWrapPos(); }
                        ImGui::EndTooltip();
                    }
                    ImGui::PopID();
                }
                ImGui::SetCursorScreenPos({origin.x,origin.y+row*pitch});
                ImGui::Dummy({available,tile});
            }
            if (filtered.empty()) ImGui::TextDisabled(catalog.empty() ? "Blocks unavailable" : "No matching blocks");
        }
        ImGui::EndChild(); ImGui::PopStyleVar(2);
        return changed;
    }
}
