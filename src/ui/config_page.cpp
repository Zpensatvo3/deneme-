#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "config_page.hpp"
#include "config_page_model.hpp"
#include "../gui/gui.hpp"
#include "../gui/control_draw.hpp"
#include "../gui/popup_controls.hpp"
#include "../gui/ui_state.hpp"
#include "../modules/core/module_registry.hpp"
#include "../modules/core/public_config.hpp"
#include "../friends/friends.hpp"
#include <cfloat>
#include <cmath>

namespace gui::config_page
{
    namespace
    {
        enum class operation { none, refresh, publish, load, remove };
        struct page_state
        {
            bool requested = false, focus_name = false;
            int selected = 0, editing = 0, deleting = 0, saving = 0, pending_id = 0;
            float expansion = 0.f;
            model::collection tab = model::collection::mine;
            std::unordered_set<int> favourites;
            char name[49]{}, description[181]{};
            operation pending = operation::none;
            std::uint64_t started_revision = 0;
            std::string confirm_name;
        };
        page_state state;
        constexpr ImU32 primary = IM_COL32(234, 234, 241, 255);
        constexpr ImU32 muted = IM_COL32(166, 163, 178, 255);
        constexpr ImU32 faint = IM_COL32(146, 142, 159, 255);
        constexpr ImU32 danger = IM_COL32(240, 112, 128, 255);

        ImFont* font(bool bold = false)
        {
            auto* result = bold ? gui::ui_bold_font : gui::ui_font;
            return result ? result : ImGui::GetFont();
        }
        ImU32 tint(ImU32 color, float opacity)
        {
            auto value = ImGui::ColorConvertU32ToFloat4(color);
            value.w *= opacity;
            return ImGui::GetColorU32(value);
        }
        ImU32 accent() { return gui::get_dynamic_color(); }
        void label(ImVec2 at, std::string_view value, float size = 12.f,
            ImU32 color = muted, bool bold = false, float width = FLT_MAX)
        {
            size = typography::readable(size);
            std::string display(value);
            if (font(bold)->CalcTextSizeA(size, FLT_MAX, 0, display.c_str()).x > width) {
                const auto dots = font(bold)->CalcTextSizeA(size, FLT_MAX, 0, "...").x;
                while (!display.empty()) {
                    auto end = display.size() - 1;
                    while (end && (static_cast<unsigned char>(display[end]) & 0xc0) == 0x80) --end;
                    display.resize(end);
                    if (font(bold)->CalcTextSizeA(size, FLT_MAX, 0, display.c_str()).x + dots <= width) break;
                }
                display += "...";
            }
            const ImVec4 clip(at.x, at.y, at.x + width, at.y + size + 4);
            ImGui::GetWindowDrawList()->AddText(font(bold), size, at, tint(color, 1.f),
                display.c_str(), nullptr, 0.f, &clip);
        }
        void tooltip(const char* text)
        {
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s", text);
        }
        void wrapped(const char* text, ImU32 color = muted)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::PushTextWrapPos(0);
            ImGui::TextUnformatted(text);
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();
        }
        enum class icon { load, save, remove, refresh, star };
        void draw_icon(ImVec2 c, icon glyph, ImU32 color)
        {
            auto* draw = ImGui::GetWindowDrawList();
            const char* symbol = nullptr;
            ImWchar codepoint = 0;
            switch (glyph) {
            case icon::load: symbol = "\xEF\x80\x99"; codepoint = 0xF019; break;
            case icon::save: symbol = "\xEF\x83\x87"; codepoint = 0xF0C7; break;
            case icon::remove: symbol = "\xEF\x87\xB8"; codepoint = 0xF1F8; break;
            case icon::refresh: symbol = "\xEF\x80\xA1"; codepoint = 0xF021; break;
            case icon::star: symbol = "\xEF\x80\x85"; codepoint = 0xF005; break;
            }
            if (gui::icon_font && gui::icon_font->FindGlyphNoFallback(codepoint)) {
                gui::control_draw::centered_label(draw, gui::icon_font, 14.f,
                    ImVec2(c.x - 8, c.y - 8), ImVec2(c.x + 8, c.y + 8), color, symbol, ImVec2(0, 0));
                return;
            }
            // Keep actions recognizable while the shared icon font is unavailable.
            if (glyph == icon::load) {
                draw->AddLine(ImVec2(c.x, c.y - 6), ImVec2(c.x, c.y + 3), color, 1.4f);
                draw->AddLine(ImVec2(c.x - 4, c.y), ImVec2(c.x, c.y + 4), color, 1.4f);
                draw->AddLine(ImVec2(c.x + 4, c.y), ImVec2(c.x, c.y + 4), color, 1.4f);
                draw->AddLine(ImVec2(c.x - 6, c.y + 5), ImVec2(c.x - 6, c.y + 7), color, 1.3f);
                draw->AddLine(ImVec2(c.x - 6, c.y + 7), ImVec2(c.x + 6, c.y + 7), color, 1.3f);
                draw->AddLine(ImVec2(c.x + 6, c.y + 7), ImVec2(c.x + 6, c.y + 5), color, 1.3f);
            } else if (glyph == icon::save) {
                draw->AddRect(ImVec2(c.x - 5, c.y - 6), ImVec2(c.x + 5, c.y + 6), color, 1.f, 0, 1.3f);
                draw->AddRect(ImVec2(c.x - 2, c.y - 6), ImVec2(c.x + 2, c.y - 1), color, 0, 0, 1.1f);
                draw->AddRect(ImVec2(c.x - 3, c.y + 2), ImVec2(c.x + 3, c.y + 6), color, 0, 0, 1.1f);
            } else if (glyph == icon::remove) {
                draw->AddLine(ImVec2(c.x - 6, c.y - 4), ImVec2(c.x + 6, c.y - 4), color, 1.3f);
                draw->AddLine(ImVec2(c.x - 2, c.y - 7), ImVec2(c.x + 2, c.y - 7), color, 1.3f);
                draw->AddRect(ImVec2(c.x - 4, c.y - 3), ImVec2(c.x + 4, c.y + 6), color, 1.f, 0, 1.3f);
                for (int i : {-1, 1}) draw->AddLine(ImVec2(c.x + i * 1.5f, c.y), ImVec2(c.x + i * 1.5f, c.y + 3), color, 1.f);
            } else if (glyph == icon::refresh) {
                draw->PathArcTo(c, 5.5f, .65f, 5.8f);
                draw->PathStroke(color, 0, 1.4f);
                draw->AddTriangleFilled(ImVec2(c.x + 7, c.y - 5), ImVec2(c.x + 2, c.y - 5), ImVec2(c.x + 6, c.y), color);
            } else {
                for (int i = 0; i < 10; ++i) {
                    const auto angle = -1.5707963f + i * .6283185f;
                    const auto radius = i % 2 ? 3.f : 6.f;
                    draw->PathLineTo(ImVec2(c.x + std::cos(angle) * radius, c.y + std::sin(angle) * radius));
                }
                draw->PathStroke(color, ImDrawFlags_Closed, 1.3f);
            }
        }
        bool icon_button(const char* id, icon glyph, const char* hint, bool active = false)
        {
            const auto p = ImGui::GetCursorScreenPos();
            const bool pressed = ImGui::InvisibleButton(id, ImVec2(30, 30));
            const bool hovered = ImGui::IsItemHovered();
            const bool held = ImGui::IsItemActive();
            const auto tone = glyph == icon::remove ? danger : accent();
            const auto maximum = ImVec2(p.x + 30, p.y + 30);
            auto* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(p, maximum, tint(hovered || active ? tone : IM_COL32_WHITE,
                held ? .19f : hovered || active ? .11f : .022f), 6.f);
            draw->AddRect(p, maximum, tint(hovered || active ? tone : IM_COL32_WHITE,
                hovered || active ? .25f : .035f), 6.f);
            draw_icon(ImVec2(p.x + 15, p.y + 15), glyph,
                tint(hovered || active ? tone : IM_COL32(163, 159, 177, 255), 1.f));
            tooltip(hint);
            return pressed;
        }
        bool button(const char* id, const char* text, ImVec2 size, bool strong = false, bool destructive = false)
        {
            const auto p = ImGui::GetCursorScreenPos();
            const bool pressed = ImGui::InvisibleButton(id, size);
            const bool hover = ImGui::IsItemHovered();
            const auto tone = destructive ? danger : accent();
            auto* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y),
                strong || destructive ? tint(tone, hover ? .25f : .16f) : tint(IM_COL32_WHITE, hover ? .07f : .035f), 8.f);
            draw->AddRect(p, ImVec2(p.x + size.x, p.y + size.y), tint(strong || destructive ? tone : IM_COL32_WHITE, strong ? .35f : .10f), 8.f);
            control_draw::centered_label(draw, font(true), typography::control, p, ImVec2(p.x + size.x, p.y + size.y),
                tint(strong || destructive ? tone : primary, 1.f), text, ImVec2(10, 4));
            return pressed;
        }
        void track(operation op, const configs::snapshot& library, int id = 0)
        {
            state.pending = op;
            state.pending_id = id;
            state.started_revision = library.revision;
        }
        void clear_editor()
        {
            state.editing = 0;
            state.name[0] = state.description[0] = '\0';
        }
        void open_editor(const configs::public_config& entry)
        {
            state.editing = entry.id;
            state.focus_name = true;
            model::copy_utf8(state.name, entry.name);
            model::copy_utf8(state.description, entry.description);
        }
        void publish(const configs::snapshot& library, int id, const std::string& name, const std::string& description)
        {
            const auto author = friends::local_player_name();
            if (author.empty() || model::trim(name).empty()) return;
            track(operation::publish, library, id);
            configs::publish(id, model::trim(name), model::trim(description), author,
                modules::export_public_config(), modules::enabled_module_count());
        }
        void render_confirmation(const configs::snapshot& library)
        {
            if ((state.deleting || state.saving) && !ImGui::IsPopupOpen("##config_confirm")) ImGui::OpenPopup("##config_confirm");
            ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(.5f, .5f));
            ImGui::SetNextWindowSize(ImVec2(std::min(420.f, ImGui::GetMainViewport()->WorkSize.x - 32.f), 0), ImGuiCond_Always);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(22, 20));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14);
            ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 14);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 10));
            ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(.060f, .057f, .075f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1, 1, 1, .10f));
            if (ImGui::BeginPopupModal("##config_confirm", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar)) {
              {
                typography::scoped_font body_font(font());
                const bool deleting = state.deleting != 0;
                if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
                    state.deleting = state.saving = 0;
                    ImGui::CloseCurrentPopup();
                }
                {
                    typography::scoped_font title_font(font(true), typography::title);
                    wrapped(deleting ? "Delete config?" : "Update config?", primary);
                }
                wrapped(state.confirm_name.c_str(), primary);
                wrapped(deleting ? "This permanently removes the public config."
                    : "Replace this config's shared settings with your current module settings?");
                ImGui::Dummy(ImVec2(0, 4));
                const auto half = (ImGui::GetContentRegionAvail().x - 8) / 2;
                if (button("cancel_confirm", "Cancel", ImVec2(half, 40))) {
                    state.deleting = state.saving = 0;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine(0, 8);
                const int id = deleting ? state.deleting : state.saving;
                const auto owned = std::find_if(library.entries.begin(), library.entries.end(), [id](const auto& entry) { return entry.id == id && entry.mine; });
                ImGui::BeginDisabled(library.busy || state.pending != operation::none || owned == library.entries.end()
                    || (!deleting && friends::local_player_name().empty()));
                if (button("confirm_action", deleting ? "Delete config" : "Save changes", ImVec2(half, 40), !deleting, deleting)) {
                    if (deleting) {
                        track(operation::remove, library, id);
                        configs::remove(id);
                    } else if (owned != library.entries.end()) publish(library, id, owned->name, owned->description);
                    state.deleting = state.saving = 0;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndDisabled();
              }
                ImGui::EndPopup();
            }
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(5);
        }
        void render_creator(const configs::snapshot& library)
        {
            const bool busy = library.busy || state.pending != operation::none;
            const auto width = ImGui::GetContentRegionAvail().x;
            const auto p = ImGui::GetCursorScreenPos();
            label(p, state.editing ? "Edit config" : "New config", 11.f);
            ImGui::Dummy(ImVec2(0, 22));
            ImGui::BeginDisabled(busy);
            if (state.focus_name) { ImGui::SetKeyboardFocusHere(); state.focus_name = false; }
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputTextWithHint("##name", "Enter config name...", state.name, IM_ARRAYSIZE(state.name))
                && model::trim(state.name).empty()) clear_editor();
            ImGui::EndDisabled();

            const bool named = !model::trim(state.name).empty();
            const float target = named ? 1.f : 0.f;
            state.expansion = gui::ui_state::get_preferences().reduce_motion ? target
                : state.expansion + (target - state.expansion) * (1.f - std::exp(-14.f * ImGui::GetIO().DeltaTime));
            if (std::abs(state.expansion - target) < .002f) state.expansion = target;
            const float eased = state.expansion * state.expansion * (3.f - 2.f * state.expansion);
            if (eased <= 0.f) return;
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            if (ImGui::BeginChild("##create_expansion", ImVec2(width, 174.f * eased), ImGuiChildFlags_None,
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * eased);
                ImGui::BeginDisabled(busy || !named || eased < .99f);
                label(ImGui::GetCursorScreenPos(), "Description (optional)", 11.f);
                ImGui::Dummy(ImVec2(0, 22));
                ImGui::InputTextMultiline("##description", state.description, IM_ARRAYSIZE(state.description), ImVec2(-1, 54));
                label(ImGui::GetCursorScreenPos(), "Shared publicly with your current settings.", 10.f, faint, false, width);
                ImGui::Dummy(ImVec2(0, 24));
                ImGui::BeginDisabled(friends::local_player_name().empty());
                if (button("publish", state.editing ? "Save changes" : "Create config", ImVec2(152, 38)))
                    publish(library, state.editing, state.name, state.description);
                tooltip(friends::local_player_name().empty() ? "Join a world to create a config." : "Share your current module settings.");
                ImGui::EndDisabled();
                ImGui::SameLine(0, 8);
                if (button("cancel_edit", "Cancel", ImVec2(92, 38))) clear_editor();
                ImGui::EndDisabled();
                ImGui::PopStyleVar();
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
        }
        void render_list(const configs::snapshot& library)
        {
            const auto width = ImGui::GetContentRegionAvail().x;
            const auto heading = ImGui::GetCursorScreenPos();
            const char* collections[] = { "Public Configs", "My Configs", "Favourites" };
            const float selector_width = std::min(200.f, std::max(1.f, width - 42.f));
            const float selector_height = ImGui::GetFrameHeight();
            auto* draw = ImGui::GetWindowDrawList();
            ImGui::SetNextItemWidth(selector_width);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(.09f, .085f, .11f, .9f));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(.13f, .12f, .17f, .95f));
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(.14f, .12f, .19f, .95f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f);
            bool open = false;
            {
              popup_controls::style popup_style;
              ImGui::SetNextWindowSizeConstraints(ImVec2(selector_width, 0), ImVec2(selector_width, FLT_MAX));
              open = ImGui::BeginCombo("##collection", "", ImGuiComboFlags_NoArrowButton);
            if (open) {
              {
                typography::scoped_font choice_font(font());
                for (int i = 0; i < 3; ++i) {
                    const bool selected = static_cast<int>(state.tab) == i;
                    ImGui::PushID(i);
                    if (popup_controls::choice("##collection_row", collections[i], selected, font(), ImGui::ColorConvertU32ToFloat4(accent())))
                        state.tab = static_cast<model::collection>(i);
                    ImGui::PopID();
                }
              }
                ImGui::EndCombo();
            }
            }
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(3);
            label(ImVec2(heading.x + 12, heading.y + (selector_height - typography::control) * .5f),
                collections[static_cast<int>(state.tab)], typography::control, open ? accent() : primary, true, selector_width - 39.f);
            const ImVec2 chevron(heading.x + selector_width - 15.f, heading.y + selector_height * .5f);
            const float direction = open ? -1.f : 1.f;
            const ImVec2 points[] = { { chevron.x - 3.5f, chevron.y - direction * 1.75f },
                { chevron.x, chevron.y + direction * 1.75f }, { chevron.x + 3.5f, chevron.y - direction * 1.75f } };
            draw->AddPolyline(points, 3, tint(open ? accent() : muted, 1.f), 0, 1.5f);
            ImGui::SetCursorScreenPos(ImVec2(heading.x + width - 30, heading.y));
            ImGui::BeginDisabled(library.busy || state.pending != operation::none);
            if (icon_button("refresh", icon::refresh, "Refresh configs")) {
                track(operation::refresh, library);
                configs::refresh();
            }
            ImGui::EndDisabled();
            ImGui::SetCursorScreenPos(ImVec2(heading.x, heading.y + selector_height + 10));
            const auto entries = model::filter(library, state.tab, "", state.favourites, model::order::newest);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
            if (ImGui::BeginChild("##rows", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar)) {
                for (const auto* entry : entries) {
                    ImGui::PushID(entry->id);
                    const auto row = ImGui::GetCursorScreenPos();
                    const auto row_width = ImGui::GetContentRegionAvail().x;
                    const auto actions_width = entry->mine ? 102.f : 72.f;
                    const bool busy = library.busy || state.pending != operation::none;
                    auto* draw = ImGui::GetWindowDrawList();
                    draw->AddRectFilled(row, ImVec2(row.x + row_width, row.y + 41), tint(IM_COL32(26, 25, 32, 230), 1.f), 6.f);
                    ImGui::BeginDisabled(busy);
                    if (ImGui::InvisibleButton("name", ImVec2(std::max(1.f, row_width - actions_width), 41)) && entry->mine) open_editor(*entry);
                    const bool hovered = ImGui::IsItemHovered();
                    if (hovered) draw->AddRectFilled(row, ImVec2(row.x + row_width, row.y + 41), tint(IM_COL32_WHITE, .025f), 6.f);
                    label(ImVec2(row.x + 12, row.y + 12), entry->name, typography::control,
                        state.selected == entry->id || state.editing == entry->id ? accent() : primary, true, row_width - actions_width - 20);
                    const auto hint = entry->name + "\nby " + entry->author_name + (entry->description.empty() ? "" : "\n" + entry->description)
                        + (entry->mine ? "\nClick to edit. Right-click for favourites." : "");
                    tooltip(hint.c_str());
                    if (ImGui::BeginPopupContextItem("##config_actions")) {
                        const bool starred = state.favourites.contains(entry->id);
                        if (ImGui::MenuItem(starred ? "Remove favourite" : "Add to session favourites")) {
                            if (starred) state.favourites.erase(entry->id); else state.favourites.insert(entry->id);
                        }
                        ImGui::EndPopup();
                    }
                    ImGui::SetCursorScreenPos(ImVec2(row.x + row_width - actions_width, row.y + 5));
                    if (icon_button("load", icon::load, "Load this config (replaces current settings)")) {
                        track(operation::load, library, entry->id);
                        configs::load(entry->id);
                    }
                    ImGui::SameLine(0, 4);
                    if (entry->mine) {
                        ImGui::BeginDisabled(friends::local_player_name().empty());
                        if (icon_button("save", icon::save, "Update with current settings")) {
                            state.saving = entry->id;
                            state.confirm_name = entry->name;
                        }
                        ImGui::EndDisabled();
                        ImGui::SameLine(0, 4);
                        if (icon_button("delete", icon::remove, "Delete config")) {
                            state.deleting = entry->id;
                            state.confirm_name = entry->name;
                        }
                    } else {
                        const bool starred = state.favourites.contains(entry->id);
                        if (icon_button("favourite", icon::star, starred ? "Remove favourite" : "Add to session favourites", starred)) {
                            if (starred) state.favourites.erase(entry->id); else state.favourites.insert(entry->id);
                        }
                    }
                    ImGui::EndDisabled();
                    ImGui::SetCursorScreenPos(ImVec2(row.x, row.y + 47));
                    ImGui::PopID();
                }
                if (entries.empty()) {
                    ImGui::Dummy(ImVec2(0, 12));
                    wrapped(library.busy ? "Fetching configs..." : state.tab == model::collection::favourites
                        ? "No favourites yet." : state.tab == model::collection::community
                        ? "No public configs yet." : "You haven't created any configs yet.");
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }
    }

    void render(bool embedded)
    {
        ImGui::PushID("config_workspace");
        std::string payload, downloaded_name;
        if (configs::consume_pending_payload(payload, downloaded_name)) {
            std::string result;
            const bool applied = modules::apply_public_config(payload, result);
            if (applied) state.selected = state.pending_id;
        }
        auto library = configs::get_snapshot();
        if (!state.requested && !library.busy) {
            track(operation::refresh, library);
            configs::refresh();
            state.requested = true;
            library = configs::get_snapshot();
        }
        if (state.pending != operation::none && library.revision > state.started_revision && !library.busy) {
            if (library.success) {
                if (state.pending == operation::publish) {
                    clear_editor();
                    state.tab = model::collection::mine;
                } else if (state.pending == operation::remove) {
                    if (state.editing == state.pending_id) clear_editor();
                    if (state.selected == state.pending_id) state.selected = 0;
                    state.favourites.erase(state.pending_id);
                }
            }
            state.pending = operation::none;
        }
        if (!embedded) gui::page_header("Configs", "Save and share your setups");
        // Keep the native input/body widgets consistent with custom-drawn text.
        typography::scoped_font workspace_font(font());
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 10));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 9.f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 6));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18, 18));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(.10f, .095f, .12f, .85f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(.13f, .12f, .15f, .9f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(.14f, .13f, .17f, .95f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1, 1, 1, .055f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(.045f, .043f, .055f, .91f));
        if (ImGui::BeginChild("##config_surface", ImVec2(0, 0), ImGuiChildFlags_Border, ImGuiWindowFlags_NoScrollbar)) {
            render_creator(library);
            ImGui::Dummy(ImVec2(0, 6));
            const auto list_top = ImGui::GetCursorScreenPos();
            const float remaining = ImGui::GetContentRegionAvail().y;
            const bool show_status = library.busy || state.pending != operation::none || !library.success;
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
            if (ImGui::BeginChild("##config_list", ImVec2(0, std::max(80.f, remaining - (show_status ? 24.f : 0.f))), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar)) render_list(library);
            ImGui::EndChild();
            ImGui::PopStyleColor();
            if (show_status) {
                ImGui::SetCursorScreenPos(ImVec2(list_top.x, list_top.y + std::max(80.f, remaining - 24.f) + 5));
                label(ImGui::GetCursorScreenPos(), library.busy || state.pending != operation::none ? "Working..." : library.status,
                    11.f, library.success ? muted : danger, false, ImGui::GetContentRegionAvail().x);
                ImGui::Dummy(ImVec2(0, 15));
            }
        }
        ImGui::EndChild();
        render_confirmation(library);
        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar(5);
        ImGui::PopID();
    }
}
