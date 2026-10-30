#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "hud_editor.hpp"
#include "hud_editor_layout.hpp"
#include "../gui/gui.hpp"
#include "../gui/ui_state.hpp"
#include "../gui/ui_typography.hpp"
#include "../gui/control_draw.hpp"
#include "../gui/watermark_draw.hpp"
#include "../modules/core/module_registry.hpp"
#include "../modules/render/arraylist.hpp"
#include "../modules/visual/spotify_hud.hpp"
#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
    namespace layout = gui::hud_editor_layout;
    constexpr int overlay_count = 3;
    using snapshot = std::array<gui::ui_state::hud_placement, overlay_count>;
    constexpr const char* ids[] = { "watermark", "arraylist", "spotify_hud" };
    constexpr const char* names[] = { "Watermark", "ArrayList", "Spotify" };
    int selected = 0, dragging = -1;
    bool grid = false, snap = true, restored = false;
    ImVec2 drag_offset{};
    std::vector<snapshot> history;

    modules::spotify_hud* spotify_module()
    {
        return dynamic_cast<modules::spotify_hud*>(modules::find_module_by_id(ids[2]));
    }
    snapshot capture()
    {
        snapshot result{{ {gui::watermark_settings.read().position_x, gui::watermark_settings.read().position_y, gui::watermark_settings.read().scale},
            {gui::arraylist_settings.read().position_x, gui::arraylist_settings.read().position_y, gui::arraylist_settings.read().scale}, {22, 60, 1.2f} }};
        if (auto* spotify = spotify_module()) { result[2].x = spotify->get_pos_x(); result[2].y = spotify->get_pos_y(); }
        for (int index = 0; index < overlay_count; ++index)
            if (auto* module = modules::find_module_by_id(ids[index])) result[index].visible = module->enabled;
        return result;
    }
    void apply(const snapshot& value)
    {
        gui::watermark_settings.update([&](auto& c) { c.position_x = value[0].x; c.position_y = value[0].y; c.scale = value[0].scale; return true; });
        gui::arraylist_settings.update([&](auto& c) { c.position_x = value[1].x; c.position_y = value[1].y; c.scale = value[1].scale; return true; });
        if (auto* spotify = spotify_module()) { spotify->set_pos_x(value[2].x); spotify->set_pos_y(value[2].y); }
        for (int index = 0; index < overlay_count; ++index)
            if (auto* module = modules::find_module_by_id(ids[index])) module->enabled = value[index].visible;
    }
    void save()
    {
        auto& preferences = gui::ui_state::get_preferences();
        preferences.hud_layout = capture(); preferences.has_hud_layout = true;
        gui::ui_state::mark_preferences_dirty();
    }
    void remember()
    {
        if (history.size() >= 32) history.erase(history.begin());
        history.push_back(capture());
    }
    void undo()
    {
        if (history.empty()) return;
        apply(history.back()); history.pop_back(); save(); dragging = -1;
    }
    void reset_selected()
    {
        if (!modules::find_module_by_id(ids[selected])) return;
        remember();
        auto value = capture();
        const bool visible = value[selected].visible;
        value[selected] = gui::ui_state::preferences{}.hud_layout[selected];
        value[selected].visible = visible;
        apply(value); save();
    }
    void set_scale(float scale)
    {
        if (selected == 2 || !modules::find_module_by_id(ids[selected])) return;
        auto value = capture();
        scale = std::clamp(scale, .5f, 2.f);
        if (value[selected].scale == scale) return;
        remember(); value[selected].scale = scale; apply(value); save();
    }
    void set_position(int index, layout::point position, ImVec2 size, ImVec2 canvas)
    {
        position = layout::clamp(position, {size.x, size.y}, {canvas.x, canvas.y});
        if (index == 0) gui::watermark_settings.update([&](auto& c) { c.position_x = position.x; c.position_y = position.y; return true; });
        if (index == 1) gui::arraylist_settings.update([&](auto& c) { c.position_x = std::max(8.f, canvas.x - position.x - size.x); c.position_y = position.y; return true; });
        if (index == 2) if (auto* spotify = spotify_module()) { spotify->set_pos_x(position.x); spotify->set_pos_y(position.y); }
        save();
    }
    void draw_editor_text(ImDrawList* draw, const ImVec2& position, float size, ImU32 color, const char* value, bool bold = false)
    {
        ImFont* font = bold ? gui::ui_bold_font : gui::ui_font;
        if (!font) font = ImGui::GetFont();
        draw->AddText(font, gui::typography::readable(size), position, color, value);
    }
    bool button(const char* id, const char* label, ImVec2 position, ImVec2 size, bool active = false, bool disabled = false)
    {
        ImGui::SetCursorScreenPos(position);
        ImGui::BeginDisabled(disabled);
        const bool pressed = ImGui::InvisibleButton(id, size);
        const bool hovered = ImGui::IsItemHovered();
        const ImVec4 tint = gui::get_dynamic_color().Value;
        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(position, ImVec2(position.x + size.x, position.y + size.y),
            active ? ImGui::GetColorU32(ImVec4(tint.x, tint.y, tint.z, hovered ? .17f : .10f))
                : ImGui::GetColorU32(ImVec4(1, 1, 1, hovered ? .06f : .025f)), 6.f);
        draw->AddRect(position, ImVec2(position.x + size.x, position.y + size.y),
            active ? ImGui::GetColorU32(ImVec4(tint.x, tint.y, tint.z, .45f)) : ImGui::GetColorU32(ImVec4(1, 1, 1, .065f)), 6.f);
        ImFont* font = gui::ui_font ? gui::ui_font : ImGui::GetFont();
        gui::control_draw::centered_label(draw, font, gui::typography::control, position,
            ImVec2(position.x + size.x, position.y + size.y),
            active ? ImGui::GetColorU32(tint) : ImGui::GetColorU32(ImGuiCol_Text), label, ImVec2(3, 3));
        ImGui::EndDisabled();
        return pressed;
    }
}

void gui::hud_editor::restore_saved_layout()
{
    if (restored || !modules::find_module_by_id(ids[2])) return;
    restored = true;
    const auto& preferences = gui::ui_state::get_preferences();
    if (preferences.has_hud_layout) apply(preferences.hud_layout);
}

void gui::hud_editor::close()
{
    dragging = -1;
    gui::hud_layout_editor_active = false; gui::hud_editor_active = false;
    gui::ui_state::flush_preferences(true);
}

void gui::hud_editor::render_page()
{
    restore_saved_layout();
    ImGui::PushID("hud_workspace");
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18, 18));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 6));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(.045f, .043f, .055f, .91f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1, 1, 1, .055f));
    if (ImGui::BeginChild("##hud_surface", ImVec2(0, 0), ImGuiChildFlags_Border, ImGuiWindowFlags_NoScrollbar)) {
        auto* draw = ImGui::GetWindowDrawList();
        const auto top = ImGui::GetCursorScreenPos();
        const float width = ImGui::GetContentRegionAvail().x;
        const ImU32 primary = IM_COL32(234, 234, 241, 255);
        draw_editor_text(draw, ImVec2(top.x + 12, top.y + 8), 19.f, primary, "HUD layout", true);
        const auto list_top = ImVec2(top.x, top.y + 46);
        const bool stacked = width < 420.f;
        const float row_height = stacked ? 88.f : 54.f;
        const float row_step = row_height + 8.f;
        for (int index = 0; index < overlay_count; ++index) {
            ImGui::PushID(ids[index]);
            auto* module = modules::find_module_by_id(ids[index]);
            const ImVec2 row(list_top.x, list_top.y + index * row_step), maximum(row.x + width, row.y + row_height);
            draw->AddRectFilled(row, maximum, IM_COL32(26, 25, 32, 230), 6.f);
            ImGui::BeginDisabled(!module);
            ImGui::SetCursorScreenPos(row);
            if (ImGui::InvisibleButton("select_row", ImVec2(std::max(1.f, stacked ? width : width - 220.f), stacked ? 34.f : row_height))) selected = index;
            if (ImGui::IsItemHovered()) draw->AddRectFilled(row, maximum, ImGui::GetColorU32(ImVec4(1, 1, 1, .025f)), 6.f);
            draw->PushClipRect(ImVec2(row.x + 12, row.y), ImVec2(maximum.x - (stacked ? 12.f : 220.f), maximum.y), true);
            draw_editor_text(draw, ImVec2(row.x + 12, row.y + (stacked ? 10.f : 18.f)), 17.f, primary, names[index], true);
            draw->PopClipRect();
            const float scale = capture()[index].scale;
            const bool fixed_scale = index == 2;
            const float controls_y = row.y + (stacked ? 44.f : 10.f);
            draw_editor_text(draw, ImVec2(maximum.x - 204, controls_y + 10.f), 14.f, ImGui::GetColorU32(ImGuiCol_TextDisabled), "Scale");
            if (button("scale_down", "-", ImVec2(maximum.x - 156, controls_y), ImVec2(34, 34), false, fixed_scale || scale <= .5f)) {
                selected = index; set_scale(scale - .05f);
            }
            char scale_text[16];
            std::snprintf(scale_text, sizeof(scale_text), "%.2fx", capture()[index].scale);
            button("scale_value", scale_text, ImVec2(maximum.x - 116, controls_y), ImVec2(64, 34), false, true);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && fixed_scale)
                ImGui::SetTooltip("%s", "Spotify uses a fixed scale.");
            if (button("scale_up", "+", ImVec2(maximum.x - 46, controls_y), ImVec2(34, 34), false, fixed_scale || scale >= 2.f)) {
                selected = index; set_scale(scale + .05f);
            }
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        if (button("edit_layout", "Edit", ImVec2(top.x + width - 104, list_top.y + overlay_count * row_step + 6), ImVec2(104, 40))) {
            dragging = -1;
            gui::hud_layout_editor_active = true;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Edit HUD positions on screen.");
    }
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
    ImGui::PopID();
}

void gui::hud_editor::render(const ImGuiViewport* viewport)
{
    if (!viewport || viewport->Size.x < 1 || viewport->Size.y < 1) return;
    restore_saved_layout();
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) { close(); return; }
    if (ImGui::GetIO().KeyCtrl && !ImGui::IsMouseDown(ImGuiMouseButton_Left) && ImGui::IsKeyPressed(ImGuiKey_Z, false)) undo();
    const ImVec2 canvas = viewport->Size, origin = viewport->Pos;
    const float toolbar_width = std::min(720.f, std::max(1.f, canvas.x - 24.f));
    const int element_columns = toolbar_width >= 560.f ? 4 : 2;
    const int control_columns = toolbar_width >= 560.f ? 7 : 4;
    const int element_rows = (overlay_count + element_columns - 1) / element_columns;
    const int control_rows = (7 + control_columns - 1) / control_columns;
    const float controls_top = 50.f + element_rows * 42.f;
    const float toolbar_height = controls_top + control_rows * 42.f + 12.f;
    const ImVec2 toolbar(origin.x + (canvas.x - toolbar_width) * .5f, origin.y + std::max(0.f, canvas.y - toolbar_height - 12.f));
    const ImVec2 toolbar_max(toolbar.x + toolbar_width, toolbar.y + toolbar_height);
    ImGui::SetNextWindowPos(origin); ImGui::SetNextWindowSize(canvas); ImGui::SetNextWindowBgAlpha(0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    if (ImGui::Begin("##HudLayoutEditor", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImU32 accent = gui::get_dynamic_color();
        draw->AddRectFilled(origin, ImVec2(origin.x + canvas.x, origin.y + canvas.y), IM_COL32(0, 0, 0, 42));
        if (grid)
            for (float y = 8; y < canvas.y; y += 16)
                for (float x = 8; x < canvas.x; x += 16)
                    draw->AddCircleFilled(ImVec2(origin.x + x, origin.y + y), .65f, IM_COL32(255, 255, 255, 24), 4);
        struct item { ImVec2 position{}, size{}; int first = 0, last = 0; modules::module* module = nullptr; };
        std::array<item, overlay_count> items{};
        // Capture actual drawing bounds once. The editor never substitutes mock shapes.
        for (int index = 0; index < overlay_count; ++index)
        {
            auto& item = items[index]; item.module = modules::find_module_by_id(ids[index]);
            item.first = draw->VtxBuffer.Size;
            if (!item.module) continue;
            if (index == 0)
            {
                item.size = gui::watermark_draw::size();
                item.position = {gui::watermark_settings.read().position_x, gui::watermark_settings.read().position_y};
                gui::watermark_draw::draw(draw, item.position.x, item.position.y);
            }
            if (index == 1)
            {
                modules::arraylist::overlay_bounds bounds{};
                modules::arraylist::draw_overlay(draw, gui::arraylist_settings.read().position_x, gui::arraylist_settings.read().position_y, true, &bounds);
                item.position = {bounds.minimum_x, bounds.minimum_y}; item.size = {bounds.width(), bounds.height()};
            }
            if (index == 2) if (auto* spotify = spotify_module())
            {
                const auto bounds = spotify->get_bounds(canvas.x, canvas.y);
                item.position = {bounds.x, bounds.y}; item.size = {bounds.w, bounds.h};
                spotify->render_hud_preview(draw, item.position.x, item.position.y);
            }
            item.last = draw->VtxBuffer.Size;
        }
        for (int index = 0; index < overlay_count; ++index)
        {
            auto& item = items[index];
            if (!item.module || item.size.x < 1 || item.size.y < 1) continue;
            const ImVec2 before = item.position;
            auto bounded = layout::clamp({before.x, before.y}, {item.size.x, item.size.y}, {canvas.x, canvas.y});
            item.position = {bounded.x, bounded.y};
            ImGui::SetCursorScreenPos(ImVec2(origin.x + item.position.x, origin.y + item.position.y));
            const ImVec2 mouse = ImGui::GetIO().MousePos;
            const bool over_toolbar = mouse.x >= toolbar.x && mouse.x <= toolbar_max.x && mouse.y >= toolbar.y && mouse.y <= toolbar_max.y;
            ImGui::PushID(index);
            ImGui::BeginDisabled(dragging < 0 && over_toolbar);
            ImGui::InvisibleButton("##move_overlay", item.size);
            const bool hovered = ImGui::IsItemHovered();
            if (ImGui::IsItemActivated())
            {
                selected = index; dragging = index; remember();
                drag_offset = {mouse.x - origin.x - item.position.x, mouse.y - origin.y - item.position.y};
            }
            float guide_x = -1, guide_y = -1;
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            {
                std::vector<layout::rect> neighbours;
                for (int other = 0; other < overlay_count; ++other) if (other != index && items[other].size.x > 0)
                    neighbours.push_back({items[other].position.x, items[other].position.y, items[other].size.x, items[other].size.y});
                const auto placed = layout::place({mouse.x - origin.x - drag_offset.x, mouse.y - origin.y - drag_offset.y},
                    {item.size.x, item.size.y}, {canvas.x, canvas.y}, snap, grid, neighbours, ImGui::GetIO().KeyAlt);
                item.position = {placed.position.x, placed.position.y}; guide_x = placed.guide_x; guide_y = placed.guide_y;
            }
            if (dragging == index && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) dragging = -1;
            ImGui::EndDisabled(); ImGui::PopID();
            if (selected == index && dragging < 0 && !ImGui::GetIO().KeyCtrl && !ImGui::GetIO().WantTextInput)
            {
                ImVec2 delta{};
                const float step = ImGui::GetIO().KeyShift ? 10.f : 1.f;
                if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) delta.x -= step;
                if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) delta.x += step;
                if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) delta.y -= step;
                if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) delta.y += step;
                if (delta.x || delta.y)
                {
                    remember(); bounded = layout::clamp({item.position.x + delta.x, item.position.y + delta.y},
                        {item.size.x, item.size.y}, {canvas.x, canvas.y}); item.position = {bounded.x, bounded.y};
                }
            }
            if (item.position.x != before.x || item.position.y != before.y)
                set_position(index, {item.position.x, item.position.y}, item.size, canvas);
            const float dx = origin.x + item.position.x - before.x, dy = origin.y + item.position.y - before.y;
            for (int vertex = item.first; vertex < item.last; ++vertex)
            {
                auto& v = draw->VtxBuffer[vertex]; v.pos.x += dx; v.pos.y += dy;
                if (!item.module->enabled)
                {
                    const unsigned alpha = (v.col & IM_COL32_A_MASK) >> IM_COL32_A_SHIFT;
                    v.col = (v.col & ~IM_COL32_A_MASK) | (static_cast<unsigned>(alpha * .45f) << IM_COL32_A_SHIFT);
                }
            }
            const ImVec2 minimum(origin.x + item.position.x, origin.y + item.position.y);
            if (selected == index || hovered)
            {
                draw->AddRect(ImVec2(minimum.x - 3, minimum.y - 3), ImVec2(minimum.x + item.size.x + 3, minimum.y + item.size.y + 3),
                    selected == index ? accent : IM_COL32(255, 255, 255, 90), 5, 0, selected == index ? 1.5f : 1.f);
                if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
            }
            if (guide_x >= 0) draw->AddLine(ImVec2(origin.x + guide_x, origin.y), ImVec2(origin.x + guide_x, origin.y + canvas.y), accent);
            if (guide_y >= 0) draw->AddLine(ImVec2(origin.x, origin.y + guide_y), ImVec2(origin.x + canvas.x, origin.y + guide_y), accent);
        }
        // Fixed controls are drawn last and cannot be covered by a dragged overlay.
        draw->AddRectFilled(toolbar, toolbar_max, IM_COL32(12, 11, 16, 247), 10.f);
        draw->AddRect(toolbar, toolbar_max, IM_COL32(255, 255, 255, 18), 10.f);
        draw_editor_text(draw, ImVec2(toolbar.x + 14, toolbar.y + 18), 17.f, IM_COL32(234, 234, 241, 255), "HUD editor", true);
        if (toolbar_width >= 560.f) {
            char position[64];
            std::snprintf(position, sizeof(position), "X %.0f   Y %.0f", items[selected].position.x, items[selected].position.y);
            draw_editor_text(draw, ImVec2(toolbar.x + 140, toolbar.y + 20), 14.f, IM_COL32(139, 137, 151, 255), position);
        }
        auto* selected_module = items[selected].module;
        if (button("##reset", "Reset", ImVec2(toolbar_max.x - 170, toolbar.y + 10), ImVec2(72, 30), false, !selected_module)) reset_selected();
        if (button("##done", "Done", ImVec2(toolbar_max.x - 90, toolbar.y + 10), ImVec2(76, 30))) close();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Esc: return to the menu");

        const float element_width = std::max(1.f, (toolbar_width - 28 - 8 * (element_columns - 1)) / element_columns);
        for (int index = 0; index < overlay_count; ++index)
            if (button(ids[index], names[index], ImVec2(toolbar.x + 14 + (index % element_columns) * (element_width + 8),
                toolbar.y + 50 + (index / element_columns) * 42.f),
                ImVec2(element_width, 34), selected == index, !items[index].module)) selected = index;

        const float slot = std::max(1.f, (toolbar_width - 28 - 8 * (control_columns - 1)) / control_columns);
        const ImVec2 button_size(slot, 34);
        auto location = [&](int column) { return ImVec2(toolbar.x + 14 + (column % control_columns) * (slot + 8),
            toolbar.y + controls_top + (column / control_columns) * 42.f); };
        selected_module = items[selected].module;
        const bool scalable = selected != 2 && selected_module;
        const auto value = capture();
        const float scale = value[selected].scale;
        if (button("##visible", selected_module && selected_module->enabled ? "Visible" : "Hidden",
            location(0), button_size, selected_module && selected_module->enabled, !selected_module))
        { remember(); selected_module->enabled = !selected_module->enabled; save(); }
        if (button("##scale_down", "-", location(1), button_size, false, !scalable || scale <= .5f)) set_scale(scale - .05f);
        char scale_text[24];
        std::snprintf(scale_text, sizeof(scale_text), "%.2fx", scale);
        button("##scale_value", scale_text, location(2), button_size, false, true);
        if (button("##scale_up", "+", location(3), button_size, false, !scalable || scale >= 2.f)) set_scale(scale + .05f);
        if (button("##grid", "Grid", location(4), button_size, grid)) grid = !grid;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Show the placement grid");
        if (button("##snap", "Snap", location(5), button_size, snap)) snap = !snap;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Align to edges. Alt: free movement");
        if (button("##undo", "Undo", location(6), button_size, false, history.empty())) undo();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s", "Ctrl+Z: undo / Arrows: 1px / Shift: 10px");
    }
    ImGui::End(); ImGui::PopStyleVar(2);
}
