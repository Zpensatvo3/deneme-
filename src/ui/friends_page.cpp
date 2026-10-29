#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "friends_page.hpp"
#include "../friends/friends.hpp"
#include "../gui/gui.hpp"
#include "../gui/control_draw.hpp"
#include "../gui/popup_controls.hpp"
#include "../gui/ui_state.hpp"
#include "../modules/core/module_registry.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <string_view>

namespace gui::friends_page
{
    namespace
    {
        struct page_state
        {
            int mode = 0;
            char name[64]{}, search[64]{};
            float middle_click_animation = -1.f;
        };
        page_state state;
        constexpr ImU32 primary = IM_COL32(234, 234, 241, 255);
        constexpr ImU32 muted = IM_COL32(139, 137, 151, 255);
        constexpr ImU32 faint = IM_COL32(98, 96, 110, 255);
        constexpr ImU32 danger = IM_COL32(240, 112, 128, 255);

        ImFont* font(bool bold = false)
        {
            auto* result = bold ? gui::ui_bold_font : gui::ui_font;
            return result ? result : ImGui::GetFont();
        }
        ImU32 tint(ImU32 color, float opacity = 1.f)
        {
            auto value = ImGui::ColorConvertU32ToFloat4(color);
            value.w *= opacity;
            return ImGui::GetColorU32(value);
        }
        ImU32 accent() { return gui::get_dynamic_color(); }
        void label(ImVec2 at, const char* text, float size = 12.f, ImU32 color = muted,
            bool bold = false, float width = FLT_MAX)
        {
            size = typography::readable(size);
            const ImVec4 clip(at.x, at.y, at.x + width, at.y + size + 4);
            ImGui::GetWindowDrawList()->AddText(font(bold), size, at, tint(color), text, nullptr, 0.f, &clip);
        }
        void hint(const char* text)
        {
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s", text);
        }
        void caption(const char* text)
        {
            label(ImGui::GetCursorScreenPos(), text, 11.f);
            ImGui::Dummy(ImVec2(0, 18));
        }
        bool action(const char* id, bool remove)
        {
            const auto p = ImGui::GetCursorScreenPos();
            const bool pressed = ImGui::InvisibleButton(id, ImVec2(30, 30));
            const bool hovered = ImGui::IsItemHovered();
            const auto tone = hovered ? (remove ? danger : accent()) : IM_COL32(163, 159, 177, 255);
            auto* draw = ImGui::GetWindowDrawList();
            const ImVec2 maximum(p.x + 30, p.y + 30);
            draw->AddRectFilled(p, maximum, tint(hovered ? tone : IM_COL32_WHITE, hovered ? .11f : .022f), 6.f);
            draw->AddRect(p, maximum, tint(hovered ? tone : IM_COL32_WHITE, hovered ? .25f : .035f), 6.f);
            if (remove && gui::icon_font && gui::icon_font->FindGlyphNoFallback(0xF1F8))
                gui::control_draw::centered_label(draw, gui::icon_font, 14.f, p, maximum, tint(tone), "\xEF\x87\xB8", ImVec2(7, 7));
            else {
                const ImVec2 center(p.x + 15, p.y + 15);
                if (remove) gui::control_draw::close_icon(draw, center, tint(tone));
                else {
                    draw->AddLine(ImVec2(center.x - 5, center.y), ImVec2(center.x + 5, center.y), tint(tone), 1.5f);
                    draw->AddLine(ImVec2(center.x, center.y - 5), ImVec2(center.x, center.y + 5), tint(tone), 1.5f);
                }
            }
            hint(remove ? "Remove friend" : "Add friend");
            return pressed;
        }
        void render_settings()
        {
            label(ImGui::GetCursorScreenPos(), "Settings", 17.f, primary, true);
            ImGui::Dummy(ImVec2(0, 28));
            auto* manager = modules::find_module_by_id("friends_manager");
            const auto p = ImGui::GetCursorScreenPos();
            const float width = ImGui::GetContentRegionAvail().x;
            ImGui::BeginDisabled(!manager);
            if (ImGui::InvisibleButton("##middle_click_friends", ImVec2(width, 32)) && manager) manager->enabled = !manager->enabled;
            hint("Enable the friend manager. Aim at a player and press the mouse wheel to add or remove them.");
            const bool enabled = manager && manager->enabled;
            const float target = enabled ? 1.f : 0.f;
            auto& animation = state.middle_click_animation;
            if (animation < 0 || gui::ui_state::get_preferences().reduce_motion) animation = target;
            else animation += (target - animation) * (1.f - std::exp(-14.f * ImGui::GetIO().DeltaTime));
            auto* draw = ImGui::GetWindowDrawList();
            label(ImVec2(p.x, p.y + 9), "Middle-click friends", 12.5f, primary, false, width - 50);
            const ImVec2 track(p.x + width - 36, p.y + 7);
            const auto off = ImGui::ColorConvertU32ToFloat4(IM_COL32(43, 41, 52, 255));
            const auto on = ImGui::ColorConvertU32ToFloat4(accent());
            const auto blend = ImVec4(off.x + (on.x - off.x) * animation, off.y + (on.y - off.y) * animation,
                off.z + (on.z - off.z) * animation, 1.f);
            draw->AddRectFilled(track, ImVec2(track.x + 36, track.y + 20), ImGui::GetColorU32(blend), 10.f);
            draw->AddCircleFilled(ImVec2(track.x + 10 + animation * 16, track.y + 10), 6.5f, tint(primary));
            ImGui::EndDisabled();
            label(ImGui::GetCursorScreenPos(), "Supported combat modules skip friends.", 11.f, faint, false, width);
            ImGui::Dummy(ImVec2(0, 24));
        }
        void render_mode()
        {
            const char* modes[] = { "Name", "Server players" };
            const auto p = ImGui::GetCursorScreenPos();
            const float width = ImGui::GetContentRegionAvail().x, height = ImGui::GetFrameHeight();
            auto* draw = ImGui::GetWindowDrawList();
            ImGui::SetNextItemWidth(-1);
            bool open = false;
          {
            popup_controls::style popup_style;
            open = ImGui::BeginCombo("##friend_mode", "", ImGuiComboFlags_NoArrowButton);
            if (open) {
                for (int i = 0; i < 2; ++i) {
                    ImGui::PushID(i);
                    if (popup_controls::choice("##mode", modes[i], state.mode == i, font(), ImGui::ColorConvertU32ToFloat4(accent()))) state.mode = i;
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }
          }
            label(ImVec2(p.x + 10, p.y + (height - typography::control) * .5f), "Mode", typography::control, primary, false, width * .4f);
            const auto extent = font(true)->CalcTextSizeA(typography::control, FLT_MAX, 0, modes[state.mode]);
            label(ImVec2(p.x + width - extent.x - 34, p.y + (height - typography::control) * .5f), modes[state.mode], typography::control, primary, true);
            const ImVec2 center(p.x + width - 15, p.y + height * .5f);
            const float d = open ? -1.f : 1.f;
            const ImVec2 points[] = { {center.x - 3.5f, center.y - d * 1.75f}, {center.x, center.y + d * 1.75f}, {center.x + 3.5f, center.y - d * 1.75f} };
            draw->AddPolyline(points, 3, tint(muted), 0, 1.5f);
            ImGui::Dummy(ImVec2(0, 3));
        }
        std::string trimmed(std::string_view text)
        {
            const auto space = [](unsigned char c) { return std::isspace(c) != 0; };
            while (!text.empty() && space(static_cast<unsigned char>(text.front()))) text.remove_prefix(1);
            while (!text.empty() && space(static_cast<unsigned char>(text.back()))) text.remove_suffix(1);
            return std::string(text);
        }
        void render_input()
        {
            const float width = ImGui::GetContentRegionAvail().x;
            if (state.mode == 0) {
                caption("Add friend");
                ImGui::SetNextItemWidth(std::max(1.f, width - 38));
                const bool submitted = ImGui::InputTextWithHint("##friend_name", "Enter player name...", state.name,
                    IM_ARRAYSIZE(state.name), ImGuiInputTextFlags_EnterReturnsTrue);
                ImGui::SameLine(0, 8);
                const auto name = trimmed(state.name);
                ImGui::BeginDisabled(name.empty());
                const bool clicked = action("add_friend", false);
                ImGui::EndDisabled();
                if (!name.empty() && (submitted || clicked)) {
                    friends::add(name);
                    state.name[0] = '\0';
                }
            } else {
                caption("Find a player");
                ImGui::SetNextItemWidth(-1);
                ImGui::InputTextWithHint("##friend_search", "Search server players...", state.search, IM_ARRAYSIZE(state.search));
            }
            ImGui::Dummy(ImVec2(0, 5));
        }
        bool matches(std::string_view name, std::string_view query)
        {
            return std::search(name.begin(), name.end(), query.begin(), query.end(), [](unsigned char a, unsigned char b) {
                return std::tolower(a) == std::tolower(b);
            }) != name.end();
        }
        void render_empty()
        {
            const auto p = ImGui::GetCursorScreenPos();
            const auto size = ImGui::GetContentRegionAvail();
            const auto center = ImVec2(p.x + size.x * .5f, p.y + std::min(78.f, std::max(20.f, size.y * .3f)));
            auto* draw = ImGui::GetWindowDrawList();
            draw->AddCircle(center, 12.f, tint(faint), 0, 1.5f);
            draw->AddCircleFilled(ImVec2(center.x - 4, center.y - 3), 1.4f, tint(faint));
            draw->AddCircleFilled(ImVec2(center.x + 4, center.y - 3), 1.4f, tint(faint));
            draw->PathArcTo(ImVec2(center.x, center.y + 8), 5.f, 3.7f, 5.72f);
            draw->PathStroke(tint(faint), 0, 1.3f);
            const char* text = state.mode == 0 ? "No friends added yet" : state.search[0]
                ? "No matching players" : "Join a server to find players";
            const auto extent = font()->CalcTextSizeA(typography::caption, FLT_MAX, 0, text);
            label(ImVec2(center.x - extent.x * .5f, center.y + 25), text, 12.f, faint);
        }
        void render_rows()
        {
            const auto saved = friends::friend_names_snapshot();
            const auto players = friends::server_players_snapshot();
            int visible = 0;
            auto row = [&](const std::string& name, bool added, bool local) {
                const auto p = ImGui::GetCursorScreenPos();
                const float width = ImGui::GetContentRegionAvail().x;
                auto* draw = ImGui::GetWindowDrawList();
                draw->AddRectFilled(p, ImVec2(p.x + width, p.y + 41), tint(IM_COL32(26, 25, 32, 230)), 6.f);
                label(ImVec2(p.x + 12, p.y + 13), name.c_str(), 13.f, added ? accent() : primary, true,
                    std::max(1.f, width - (state.mode == 1 ? 116.f : 56.f)));
                if (state.mode == 1 && (local || added))
                    label(ImVec2(p.x + width - 94, p.y + 15), local ? "You" : "Friend", 10.f, muted, false, 50.f);
                ImGui::SetCursorScreenPos(ImVec2(p.x + width - 36, p.y + 5));
                ImGui::BeginDisabled(local);
                if (action("toggle_friend", added)) {
                    if (added) friends::remove(name); else friends::add(name);
                }
                ImGui::EndDisabled();
                ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + 47));
                ++visible;
            };
            if (state.mode == 0) {
                for (const auto& name : saved) {
                    ImGui::PushID(name.c_str());
                    row(name, true, false);
                    ImGui::PopID();
                }
            } else {
                for (const auto& player : players) {
                    if (state.search[0] && !matches(player.name, state.search)) continue;
                    ImGui::PushID(player.name.c_str());
                    row(player.name, player.friend_added, player.local);
                    ImGui::PopID();
                }
            }
            if (!visible) render_empty();
        }
    }

    void render(bool embedded)
    {
        typography::scoped_font workspace_font(font());
        if (!embedded) gui::page_header("Friends", "Manage your session friends");
        ImGui::PushID("friends_workspace");
        ImGui::PushFont(font());
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 7));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 6));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18, 18));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(.10f, .095f, .12f, .85f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(.13f, .12f, .15f, .9f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(.14f, .13f, .17f, .95f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1, 1, 1, .055f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(.045f, .043f, .055f, .91f));
        if (ImGui::BeginChild("##friends_surface", ImVec2(0, 0), ImGuiChildFlags_Border, ImGuiWindowFlags_NoScrollbar)) {
            render_settings();
            render_mode();
            render_input();
            caption(state.mode == 0 ? "Friend list" : "Server players");
            const auto status = friends::status_message();
            const float remaining = ImGui::GetContentRegionAvail().y;
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
            if (ImGui::BeginChild("##friends_rows", ImVec2(0, std::max(60.f, remaining - (status.empty() ? 0.f : 22.f))),
                ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar)) render_rows();
            ImGui::EndChild();
            ImGui::PopStyleColor();
            if (!status.empty()) {
                label(ImGui::GetCursorScreenPos(), status.c_str(), 10.f, faint, false, ImGui::GetContentRegionAvail().x);
                ImGui::Dummy(ImVec2(0, 14));
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar(6);
        ImGui::PopFont();
        ImGui::PopID();
    }
}
