#pragma once
#include "player_model_preview.hpp"
#include "../modules/combat/aim_regions.hpp"
#include "../modules/visual/player_esp.hpp"
#include <algorithm>
#include <vector>

namespace gui::aim_region_picker
{
    inline void render(std::vector<int>& selected)
    {
        ImGui::Text("Aim Regions  %d / 10", static_cast<int>(selected.size()));
        ImGui::TextDisabled("Click a region to select or remove it.");
        const float width = (std::max)(1.f, ImGui::GetContentRegionAvail().x);
        const float scale = (std::min)(1.f, width / 260.f);
        const float height = 270.f * scale;
        const ImVec2 start = ImGui::GetCursorScreenPos();
        const ImVec2 origin(start.x + (width - 260.f * scale) * .5f, start.y - 80.f * scale);
        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(start, ImVec2(start.x + width, start.y + height), IM_COL32(3,4,7,222), 10.f);
        gui::player_model_preview::draw(draw, origin, modules::player_esp::local_skin_texture_id(),
            0.f, IM_COL32(74,66,105,255), IM_COL32(49,45,72,255), scale);
        ImGui::Dummy(ImVec2(width, height));
        const ImVec2 after = ImGui::GetCursorScreenPos();
        ImGui::PushID("aim_region_picker");
        for (int id = 0; id < aim_regions::count; ++id) {
            const auto rect = aim_regions::region(id);
            const ImVec2 minimum(origin.x + static_cast<float>(rect.left) * scale,
                                 origin.y + static_cast<float>(rect.top) * scale);
            const ImVec2 maximum(origin.x + static_cast<float>(rect.right) * scale,
                                 origin.y + static_cast<float>(rect.bottom) * scale);
            auto found = std::find(selected.begin(), selected.end(), id);
            bool active = found != selected.end();
            ImGui::PushID(id);
            ImGui::SetCursorScreenPos(minimum);
            if (ImGui::InvisibleButton("##region", ImVec2(maximum.x - minimum.x, maximum.y - minimum.y))) {
                if (active) { selected.erase(found); active = false; }
                else if (selected.size() < aim_regions::maximum_selected) {
                    selected.push_back(id); std::sort(selected.begin(), selected.end()); active = true;
                }
            }
            const bool hovered = ImGui::IsItemHovered();
            draw->AddRectFilled(minimum, maximum, active ? IM_COL32(151,83,255,155) :
                (hovered ? IM_COL32(255,255,255,65) : IM_COL32(10,12,18,32)));
            draw->AddRect(minimum, maximum, active ? IM_COL32(190,143,255,255) : IM_COL32(210,215,230,85));
            if (hovered) ImGui::SetTooltip("%s / Region %d\n%s", aim_regions::names[id / 10], id + 1,
                active ? "Selected - click to remove" : (selected.size() == 10 ? "10 selected - remove one first" : "Click to select"));
            ImGui::PopID();
        }
        ImGui::PopID();
        ImGui::SetCursorScreenPos(after);
        if (ImGui::SmallButton("Clear regions")) selected.clear();
        ImGui::SameLine();
        if (ImGui::SmallButton("Body preset")) { selected.clear(); for (int id = 10; id < 20; ++id) selected.push_back(id); }
        if (selected.empty()) ImGui::TextDisabled("No regions selected: aim correction is off.");
        else ImGui::TextDisabled("Nearest region per click; stop once aligned.");
    }
}
