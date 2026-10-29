#include "grim_ui.hpp"
#include "hud_editor.hpp"
#include "menu_background.hpp"
#include "keybinds_page.hpp"
#include "dock_layout.hpp"
#include "module_categories.hpp"

#include "../gui/gui.hpp"
#include "../gui/module_descriptions.hpp"
#include "../gui/control_draw.hpp"
#include "../gui/ui_state.hpp"
#include "../gui/ui_typography.hpp"
#include "../modules/core/module.hpp"
#include "../modules/core/module_registry.hpp"
#include "../modules/visual/player_esp.hpp"
#include "../friends/friends.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstring>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
    ImU32 accent = IM_COL32(155, 69, 255, 255);
    ImU32 accent_hover = IM_COL32(173, 96, 255, 255);
    constexpr ImU32 primary = IM_COL32(244, 241, 247, 255);
    constexpr ImU32 secondary = IM_COL32(194, 189, 201, 255);
    constexpr ImU32 muted = IM_COL32(133, 128, 142, 255);
    constexpr ImU32 shell = IM_COL32(3, 3, 5, 178);
    // The shell already provides the glass tint. Sidebar is only a subtle
    // tonal layer so it never looks like a second opaque window inside it.
    constexpr ImU32 sidebar = IM_COL32(7, 6, 10, 54);

    enum class page
    {
        combat,
        vape,
        misc,
        visuals,
        client,
        friends,
        configs,
        hud,
        themes,
        keybinds
    };

    struct card_metrics
    {
        std::string title;
        float width = -1.f, height = 0.f, title_height = 0.f;
        ImFont* title_font = nullptr;
        ImFont* body_font = nullptr;
    };

    struct runtime_state
    {
        page current = page::combat;
        modules::module* selected = nullptr;
        modules::module* settings_module = nullptr;
        float settings_animation = 0.f;
        bool search_active = false;
        bool focus_search = false;
        ImGuiID search_input_id = 0;
        int search_selection = 0;
        char search[96]{};
        std::string search_completion_target;
        std::size_t search_completion_cursor = 0;
        double search_completion_step_at = 0.0;
        bool search_completion_active = false;
        bool search_completion_pending = false;
        float player_preview_yaw = 0.0f;
        double profile_refreshed_at = -10.0;
        std::string profile_label;
        bool had_popup = false;
        int rendered_frame = -1;
        bool positioned = false;
        ImVec2 position{};
        ImVec2 drag_grab_offset{};
        bool dragging_shell = false;
        page list_page = page::combat;
        std::string list_query;
        int list_frame = -1;
        std::unordered_map<ImGuiID, float> hover_animation;
        std::unordered_map<ImGuiID, float> nav_hover_animation;
        std::unordered_map<ImGuiID, float> nav_active_animation;
        std::unordered_map<ImGuiID, float> card_selected_animation;
        std::unordered_map<ImGuiID, float> card_enabled_animation;
        std::unordered_map<modules::module*, float> options_content_height;
        std::vector<modules::module*> catalogue, page_modules, search_modules;
        std::unordered_map<modules::module*, card_metrics> card_layouts;
        page cached_page = page::combat;
        bool page_cached = false, search_cached = false;
        std::string cached_search;
        float page_fade = 1.0f;
    } state;

    struct navigation_entry
    {
        page target;
        const char* label;
        const char* glyph;
    };

    constexpr navigation_entry feature_navigation[] = {
        { page::combat,   "Combat",   "\xEF\x81\x9B" },
        { page::misc,     "Misc",     "\xEF\x84\xAE" },
        { page::visuals,  "Visuals",  "\xEF\x81\xAE" },
        { page::client,   "Client",   "\xEF\x87\x9E" },
        { page::vape,     "Vape",     "\xEF\x83\xA7" },
    };

    constexpr navigation_entry other_navigation[] = {
        { page::friends, "Friends", "\xEF\x83\x80" },
        { page::configs, "Configs", "\xEF\x81\xBB" },
        { page::themes,  "Themes",  "\xEF\x94\xBF" }, // palette (f53f)
        { page::hud,     "Hud",     "\xEF\x84\x88" },
        { page::keybinds,"Keybinds","\xEF\x84\x9C" },
    };

    float approach(float current, float target, float speed = 12.0f)
    {
        if (gui::ui_state::get_preferences().reduce_motion) return target;
        const float dt = std::clamp(ImGui::GetIO().DeltaTime, 0.0001f, 0.05f);
        const float diff = target - current;
        if (std::abs(diff) < 0.0005f)
            return target;
        return current + diff * (1.0f - std::exp(-speed * dt));
    }

    ImU32 alpha(ImU32 color, float multiplier)
    {
        ImVec4 value = ImGui::ColorConvertU32ToFloat4(color);
        value.w *= std::clamp(multiplier, 0.0f, 1.0f);
        return ImGui::ColorConvertFloat4ToU32(value);
    }

    ImU32 blend(ImU32 a, ImU32 b, float amount)
    {
        const ImVec4 av = ImGui::ColorConvertU32ToFloat4(a);
        const ImVec4 bv = ImGui::ColorConvertU32ToFloat4(b);
        amount = std::clamp(amount, 0.0f, 1.0f);
        return ImGui::ColorConvertFloat4ToU32(ImVec4(
            av.x + (bv.x - av.x) * amount,
            av.y + (bv.y - av.y) * amount,
            av.z + (bv.z - av.z) * amount,
            av.w + (bv.w - av.w) * amount));
    }

    std::string title_case(const char* source)
    {
        std::string result = source ? source : "Unknown";
        bool upper = true;
        for (char& character : result)
        {
            if (upper && character >= 'a' && character <= 'z')
                character = static_cast<char>(character - 'a' + 'A');
            upper = character == ' ';
        }
        return result;
    }

    const char* description(const char* name)
    {
        return gui::module_description(name);
    }

    const char* page_label(page value)
    {
        switch (value)
        {
        case page::combat: return "Combat";
        case page::vape: return "Vape";
        case page::misc: return "Misc";
        case page::visuals: return "Visuals";
        case page::client: return "Client";
        case page::friends: return "Friends";
        case page::configs: return "Configs";
        case page::hud: return "Hud";
        case page::themes: return "Themes";
        case page::keybinds: return "Keybinds";
        }
        return "onestep";
    }

    const char* page_description(page value)
    {
        switch (value)
        {
        case page::combat: return "Combat assistance and timing";
        case page::vape: return "Independent combat engines";
        case page::misc: return "Movement, player and inventory tools";
        case page::visuals: return "World visualization modules";
        case page::client: return "Client interface and safety";
        case page::friends: return "Trusted players in this session";
        case page::configs: return "Browse and manage community setups";
        case page::hud: return "Drag and position your on-screen overlays";
        case page::themes: return "";
        case page::keybinds: return "Manage module shortcuts";
        }
        return "";
    }

    bool belongs_to_page(const modules::module* module, page target)
    {
        if (!module) return false;
        const auto category = gui::module_categories::display(module->get_category());
        if (target == page::combat) return category == "Combat";
        if (target == page::vape) return category == "Vape";
        if (target == page::misc) return category == "Misc";
        if (target == page::visuals) return category == "Visuals";
        if (target == page::client) return category == "Client";
        if (target == page::hud) return category == "Hud";
        return false;
    }

    bool has_settings_panel(modules::module* module)
    {
        return module && std::strcmp(module->get_id(), "panic") != 0 && module->has_custom_options();
    }

    void activate_module(modules::module* module, bool toggle_panel)
    {
        if (!module) return;
        if (std::strcmp(module->get_id(), "panic") == 0) {
            state.selected = nullptr;
            gui::request_panic();
        } else if (has_settings_panel(module)) {
            state.selected = toggle_panel && state.selected == module ? nullptr : module;
        } else {
            state.selected = nullptr;
            module->enabled = !module->enabled;
        }
    }

    bool matches(const modules::module* module, const char* query)
    {
        if (!module || !query || !*query) return true;
        std::string needle(query);
        std::string haystack = std::string(module->get_name()) + " " + description(module->get_name());
        std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        std::transform(haystack.begin(), haystack.end(), haystack.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return haystack.find(needle) != std::string::npos;
    }

    bool case_insensitive_prefix(const std::string& value, const std::string& target)
    {
        if (value.size() > target.size()) return false;
        for (std::size_t index = 0; index < value.size(); ++index)
            if (std::tolower(static_cast<unsigned char>(value[index])) !=
                std::tolower(static_cast<unsigned char>(target[index]))) return false;
        return true;
    }

    void refresh_catalogue()
    {
        const auto& registry = modules::get_modules();
        if (state.catalogue == registry) return;
        state.catalogue = registry;
        state.card_layouts.clear();
        state.page_cached = state.search_cached = false;
    }

    const std::vector<modules::module*>& search_results()
    {
        refresh_catalogue();
        if (!state.search_cached || state.cached_search != state.search)
        {
            state.search_modules.clear();
            if (state.search[0])
                for (auto* module : state.catalogue)
                    if (module && matches(module, state.search)) state.search_modules.push_back(module);
            state.cached_search = state.search;
            state.search_cached = true;
        }
        return state.search_modules;
    }

    void clear_search()
    {
        // InputText reapplies its private buffer on deactivation. Clear only
        // this field's cached text as well, so Esc/X cannot restore the query.
        if (auto* context = ImGui::GetCurrentContext(); context && state.search_input_id) {
            if (auto* input = ImGui::GetInputTextState(state.search_input_id);
                input && input->TextW.Size && input->TextA.Size) {
                input->ClearText();
                if (input->InitialTextA.Size) input->InitialTextA[0] = '\0';
            }
            if (context->InputTextDeactivatedState.ID == state.search_input_id)
                context->InputTextDeactivatedState.ID = 0;
        }
        state.search[0] = '\0';
        state.search_selection = 0;
        state.search_completion_active = false;
        state.search_completion_pending = false;
        state.search_completion_target.clear();
    }

    void open_search_result(modules::module* module)
    {
        if (!module) return;
        const char* category = module->get_category();
        if (std::strcmp(category, "Combat") == 0) state.current = page::combat;
        else if (std::strcmp(category, "Vape") == 0) state.current = page::vape;
        else if (std::strcmp(category, "Movement") == 0 || std::strcmp(category, "Player") == 0 ||
            std::strcmp(category, "World") == 0 || std::strcmp(category, "Misc") == 0) state.current = page::misc;
        else if (std::strcmp(category, "Visual") == 0) state.current = page::visuals;
        else if (std::strcmp(category, "Render") == 0) state.current = page::hud;
        else state.current = page::client;

        activate_module(module, false);
        clear_search();
        state.search_active = false;
        ImGui::ClearActiveID();
    }

    int search_completion_callback(ImGuiInputTextCallbackData* data)
    {
        if (!data) return 0;
        if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion && data->UserData)
        {
            const modules::module* module = static_cast<const modules::module*>(data->UserData);
            state.search_completion_target = module->get_name();
            state.search_completion_cursor = std::min<std::size_t>(std::strlen(data->Buf), state.search_completion_target.size());
            state.search_completion_step_at = ImGui::GetTime();
            state.search_completion_active = state.search_completion_cursor < state.search_completion_target.size();
            state.search_completion_pending = false;
        }
        else if (data->EventFlag == ImGuiInputTextFlags_CallbackAlways && state.search_completion_active &&
            ImGui::GetTime() >= state.search_completion_step_at)
        {
            state.search_completion_cursor = std::min(state.search_completion_cursor + 1, state.search_completion_target.size());
            const std::string value = state.search_completion_target.substr(0, state.search_completion_cursor);
            data->DeleteChars(0, data->BufTextLen);
            data->InsertChars(0, value.c_str());
            state.search_completion_step_at = ImGui::GetTime() + 0.028;
            state.search_completion_active = state.search_completion_cursor < state.search_completion_target.size();
        }
        return 0;
    }

    void draw_text(ImDrawList* draw, ImFont* font, float size, const ImVec2& position, ImU32 color, const char* value)
    {
        draw->AddText(font ? font : ImGui::GetFont(), gui::typography::readable(size), position, color, value ? value : "");
    }

    void icon(ImDrawList* draw, const char* glyph, const ImVec2& center, ImU32 color, float size = 16.0f)
    {
        ImFont* font = gui::icon_font ? gui::icon_font : ImGui::GetFont();
        const char* value = gui::icon_font ? glyph : "o";
        const ImVec2 extent = font->CalcTextSizeA(size, FLT_MAX, 0.0f, value);
        draw->AddText(font, size, ImVec2(center.x - extent.x * 0.5f, center.y - extent.y * 0.5f), color, value);
    }

    bool navigation_button(const navigation_entry& entry, const ImVec2& position, float width)
    {
        ImGui::PushID(static_cast<int>(entry.target));
        ImGui::SetCursorScreenPos(position);
        ImGui::InvisibleButton("##navigation", ImVec2(width, 34.0f));
        const bool clicked = ImGui::IsItemClicked();
        const bool hovered = ImGui::IsItemHovered();
        const bool selected = state.current == entry.target;
        const ImGuiID id = ImGui::GetItemID();

        float& hover = state.nav_hover_animation[id];
        hover = approach(hover, hovered ? 1.0f : 0.0f, 13.0f);

        float& active = state.nav_active_animation[id];
        active = approach(active, selected ? 1.0f : 0.0f, 14.0f);

        ImDrawList* draw = ImGui::GetWindowDrawList();
        if (active > 0.001f)
        {
            draw->AddRectFilled(position, ImVec2(position.x + width, position.y + 34.0f),
                alpha(accent, active), 7.0f);
            const float bar_h = 18.0f * active;
            const float bar_y = position.y + (34.0f - bar_h) * 0.5f;
            draw->AddRectFilled(ImVec2(position.x + 2.0f, bar_y),
                ImVec2(position.x + 5.0f, bar_y + bar_h),
                IM_COL32(255, 255, 255, static_cast<int>(240.0f * active)), 1.5f);
        }
        else if (hover > 0.001f)
        {
            draw->AddRectFilled(position, ImVec2(position.x + width, position.y + 34.0f),
                IM_COL32(255, 255, 255, static_cast<int>(12.0f * hover)), 7.0f);
        }

        const ImU32 base_col = blend(muted, secondary, hover);
        const ImU32 text_col = blend(base_col, primary, active);

        icon(draw, entry.glyph, ImVec2(position.x + 18.0f, position.y + 17.0f), text_col, 15.0f);

        ImFont* label_font = gui::ui_bold_font ? gui::ui_bold_font : ImGui::GetFont();
        constexpr float label_size = 15.5f;
        const char* label = entry.label;
        const ImVec2 label_extent = label_font->CalcTextSizeA(label_size, FLT_MAX, 0.0f, label);
        draw_text(draw, label_font, label_size,
            ImVec2(position.x + 38.0f, position.y + (34.0f - label_extent.y) * 0.5f),
            text_col, label);
        ImGui::PopID();
        if (clicked && (state.current != entry.target || state.search[0] || state.search_active))
        {
            state.current = entry.target;
            state.page_fade = 0.0f;
            clear_search();
            state.focus_search = false;
            state.search_active = false;
            ImGui::ClearActiveID();
            state.selected = nullptr;
        }
        return clicked;
    }

    constexpr float module_title_size = 17.f, module_description_size = gui::typography::caption;
    const card_metrics& measure_card(modules::module* module, float width)
    {
        auto* title_font = gui::ui_bold_font ? gui::ui_bold_font : ImGui::GetFont();
        auto* body_font = gui::ui_font ? gui::ui_font : ImGui::GetFont();
        auto& measured = state.card_layouts[module];
        if (measured.width != width || measured.title_font != title_font || measured.body_font != body_font)
        {
            const float wrap = std::max(1.f, width - 24.f);
            measured.title = title_case(module->get_name());
            measured.width = width;
            measured.title_font = title_font;
            measured.body_font = body_font;
            measured.title_height = title_font->CalcTextSizeA(module_title_size, FLT_MAX, wrap, measured.title.c_str()).y;
            const float body_height = body_font->CalcTextSizeA(module_description_size, FLT_MAX, wrap, description(module->get_name())).y;
            measured.height = std::ceil(std::max(76.f, 12.f + measured.title_height + 6.f + body_height + 12.f));
        }
        return measured;
    }

    void flowing_card_border(ImDrawList* draw, ImVec2 minimum, ImVec2 maximum, float opacity)
    {
        // Recolor one anti-aliased rounded stroke: no segment seams or per-card allocations.
        const int first = draw->VtxBuffer.Size;
        draw->AddRect(minimum, maximum, alpha(accent, opacity), 8.f, 0, 1.35f);
        const float width = maximum.x - minimum.x, height = maximum.y - minimum.y;
        const float perimeter = std::max(1.f, 2.f * (width + height));
        const double phase = gui::ui_state::get_preferences().reduce_motion ? 0. : ImGui::GetTime() * .22;
        for (int index = first; index < draw->VtxBuffer.Size; ++index)
        {
            auto& vertex = draw->VtxBuffer[index];
            const float x = std::clamp(vertex.pos.x - minimum.x, 0.f, width);
            const float y = std::clamp(vertex.pos.y - minimum.y, 0.f, height);
            const float edge = std::min({x, y, width - x, height - y});
            const float distance = edge == y ? x : edge == width - x ? width + y
                : edge == height - y ? 2.f * width + height - x : perimeter - y;
            const float wave = .5f + .5f * static_cast<float>(std::cos(6.28318530718 * (distance / perimeter - phase)));
            const ImU32 color = blend(alpha(accent, 1.f), blend(accent_hover, IM_COL32_WHITE, .65f), wave * wave);
            vertex.col = (color & ~IM_COL32_A_MASK) | (vertex.col & IM_COL32_A_MASK);
        }
    }

    void render_card(modules::module* module, float width, float height, bool search_selected = false)
    {
        if (!module) return;
        const ImVec2 minimum = ImGui::GetCursorScreenPos();
        const ImVec2 maximum(minimum.x + width, minimum.y + height);
        if (!ImGui::IsRectVisible(minimum, maximum))
        {
            // Keep the full scroll extent/keyboard reveal target, but do not
            // tessellate rounded cards and text outside the child's clip rect.
            ImGui::Dummy(ImVec2(width, height));
            return;
        }
        ImGui::PushID(module->get_id());
        const bool clicked = ImGui::InvisibleButton("##module_card", ImVec2(width, height));
        const ImGuiID id = ImGui::GetItemID();
        const bool hovered = ImGui::IsItemHovered();

        float& hover = state.hover_animation[id];
        hover = approach(hover, hovered ? 1.0f : 0.0f, 13.0f);

        const bool is_selected = (state.selected == module) || search_selected;
        float& sel_anim = state.card_selected_animation[id];
        sel_anim = approach(sel_anim, is_selected ? 1.0f : 0.0f, 14.0f);

        float& en_anim = state.card_enabled_animation[id];
        en_anim = approach(en_anim, module->enabled ? 1.0f : 0.0f, 14.0f);

        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(minimum, maximum,
            blend(IM_COL32(29, 28, 33, 235), IM_COL32(36, 34, 41, 245), hover), 8.f);
        if (sel_anim > .001f)
            flowing_card_border(draw, minimum, maximum, .85f * sel_anim);

        const auto& measured = measure_card(module, width);
        const float title_y = minimum.y + 12.f;
        const float wrap = std::max(1.f, width - 24.f);
        const ImU32 title_col = blend(primary, accent_hover, en_anim);
        const ImVec4 title_clip(minimum.x + 12.f, minimum.y, maximum.x - 12.f, maximum.y);
        draw->AddText(measured.title_font, module_title_size,
            ImVec2(minimum.x + 12.f, title_y), title_col, measured.title.c_str(), nullptr, wrap, &title_clip);

        const float description_y = title_y + measured.title_height + 6.f;
        const ImVec4 clip(minimum.x + 12.f, description_y, maximum.x - 12.f, maximum.y - 8.f);
        draw->AddText(gui::ui_font ? gui::ui_font : ImGui::GetFont(), module_description_size,
            ImVec2(minimum.x + 12.f, description_y), IM_COL32(177, 172, 188, 255), description(module->get_name()), nullptr, wrap, &clip);

        if (clicked) activate_module(module, true);
        ImGui::PopID();
    }

    const std::vector<modules::module*>& visible_modules()
    {
        if (state.search[0]) return search_results();
        refresh_catalogue();
        if (!state.page_cached || state.cached_page != state.current)
        {
            state.page_modules.clear();
            for (auto* module : state.catalogue)
                if (belongs_to_page(module, state.current)) state.page_modules.push_back(module);
            state.cached_page = state.current;
            state.page_cached = true;
        }
        return state.page_modules;
    }

    void render_grid(const ImVec2& minimum, const ImVec2& size)
    {
        ImGui::SetCursorScreenPos(minimum);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.f, 12.f));
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(.045f, .043f, .055f, .91f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.f);
        if (state.list_frame != ImGui::GetFrameCount() - 1 || state.list_page != state.current || state.list_query != state.search)
            ImGui::SetNextWindowScroll(ImVec2(0, 0));
        state.list_page = state.current;
        state.list_query = state.search;
        state.list_frame = ImGui::GetFrameCount();
        if (ImGui::BeginChild("##grim_module_grid", size, ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollbar))
        {
            const float width = ImGui::GetContentRegionAvail().x;
            const auto& visible = visible_modules();
            const ImVec2 list_top = ImGui::GetCursorScreenPos();
            constexpr float gap = 8.f;
            const int columns = width >= 560.f ? 2 : 1;
            const float card_width = (width - gap * (columns - 1)) / columns;
            float row_y = list_top.y;
            for (std::size_t row = 0; row < visible.size(); row += columns)
            {
                float height = 76.f;
                for (int column = 0; column < columns && row + column < visible.size(); ++column)
                    height = std::max(height, measure_card(visible[row + column], card_width).height);
                for (int column = 0; column < columns && row + column < visible.size(); ++column) {
                    const auto index = row + column;
                    ImGui::SetCursorScreenPos(ImVec2(list_top.x + column * (card_width + gap), row_y));
                    render_card(visible[index], card_width, height,
                        state.search[0] && static_cast<int>(index) == state.search_selection);
                    if (state.search[0] && static_cast<int>(index) == state.search_selection &&
                        (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_UpArrow)))
                        ImGui::SetScrollHereY(.5f);
                }
                row_y += height + gap;
            }
            if (visible.empty())
            {
                ImGui::Dummy(ImVec2(0.0f, 70.0f));
                const char* empty = state.search[0] ? "No matching modules" : "No modules in this category";
                const float text_width = ImGui::CalcTextSize(empty).x;
                ImGui::SetCursorPosX(std::max(0.0f, (ImGui::GetContentRegionAvail().x - text_width) * 0.5f));
                ImGui::TextColored(ImVec4(0.52f, 0.50f, 0.56f, 1.0f), "%s", empty);
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);
    }


    void render_hud_editor_launcher(const ImVec2& minimum, const ImVec2& size)
    {
        ImGui::SetCursorScreenPos(minimum);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
        if (ImGui::BeginChild("##hud_page", size, ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar))
            gui::hud_editor::render_page();
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    void render_search(const ImVec2& position, float width)
    {
        auto results = search_results();
        state.search_selection = results.empty() ? 0 : std::clamp(state.search_selection, 0, static_cast<int>(results.size()) - 1);
        if (state.search_completion_pending && ImGui::GetTime() >= state.search_completion_step_at) {
            state.search_completion_pending = false;
            state.search_completion_active = true;
        }
        const std::string query_before = state.search;
        const bool completion_was_active = state.search_completion_active;
        modules::module* completion = results.empty() ? nullptr : results[static_cast<std::size_t>(state.search_selection)];

        ImGui::SetCursorScreenPos(position);
        ImGui::PushFont(gui::ui_font ? gui::ui_font : ImGui::GetFont());
        const float previous_scale = ImGui::GetCurrentWindow()->FontWindowScale;
        ImGui::SetWindowFontScale(previous_scale * 14.f / ImGui::GetFontSize());
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(34.f, 11.f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(.045f, .04f, .065f, state.search_active ? .52f : .32f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(.06f, .05f, .08f, .42f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(.06f, .05f, .08f, .52f));
        ImGui::PushStyleColor(ImGuiCol_Border, state.search_active
            ? ImGui::ColorConvertU32ToFloat4(alpha(accent, .6f)) : ImVec4(1.f, 1.f, 1.f, .10f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(primary));
        ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImGui::ColorConvertU32ToFloat4(muted));
        if (state.focus_search) {
            ImGui::SetKeyboardFocusHere();
            state.focus_search = false;
        }
        ImGui::SetNextItemWidth(width);
        const bool submitted = ImGui::InputTextWithHint("##sidebar_search", "Search", state.search, IM_ARRAYSIZE(state.search),
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackAlways,
            search_completion_callback, completion);
        state.search_input_id = ImGui::GetItemID();
        state.search_active = ImGui::IsItemActive();
        if (!state.search_active) {
            state.search_completion_pending = false;
            state.search_completion_active = false;
        }
        ImGui::PopStyleColor(6);
        ImGui::PopStyleVar(3);
        ImGui::SetWindowFontScale(previous_scale);
        ImGui::PopFont();

        auto* draw = ImGui::GetWindowDrawList();
        const ImVec2 center(position.x + 17.f, position.y + 16.f);
        const ImU32 icon_color = state.search_active ? secondary : muted;
        draw->AddCircle(center, 5.f, icon_color, 0, 1.6f);
        draw->AddLine(ImVec2(center.x + 3.5f, center.y + 3.5f),
            ImVec2(center.x + 7.5f, center.y + 7.5f), icon_color, 1.6f);
        if (state.search[0]) {
            const ImVec2 clear_min(position.x + width - 28.f, position.y + 6.f);
            const ImVec2 clear_max(clear_min.x + 22.f, clear_min.y + 24.f);
            const bool clear_hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)
                && ImGui::IsMouseHoveringRect(clear_min, clear_max);
            if (clear_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                clear_search();
                state.search_active = false;
                state.focus_search = true;
                ImGui::ClearActiveID();
            }
            const ImVec2 c(position.x + width - 17.f, position.y + 18.f);
            const auto color = clear_hovered ? primary : muted;
            draw->AddLine(ImVec2(c.x - 3, c.y - 3), ImVec2(c.x + 3, c.y + 3), color, 1.3f);
            draw->AddLine(ImVec2(c.x - 3, c.y + 3), ImVec2(c.x + 3, c.y - 3), color, 1.3f);
        }
        if (query_before != state.search) {
            state.search_selection = 0;
            results = search_results();
            if (query_before.empty() && state.search[0]) state.selected = nullptr;
            if (!completion_was_active && !results.empty()) {
                const std::string target = results.front()->get_name();
                if (case_insensitive_prefix(state.search, target) && std::strlen(state.search) < target.size()) {
                    state.search_completion_target = target;
                    state.search_completion_cursor = std::strlen(state.search);
                    state.search_completion_step_at = ImGui::GetTime() + .34;
                    state.search_completion_pending = true;
                } else {
                    state.search_completion_pending = false;
                    state.search_completion_active = false;
                }
            } else if (results.empty()) {
                state.search_completion_pending = false;
                state.search_completion_active = false;
            }
        }
        if (!results.empty()) {
            if (state.search_active && ImGui::IsKeyPressed(ImGuiKey_DownArrow))
                state.search_selection = (state.search_selection + 1) % static_cast<int>(results.size());
            if (state.search_active && ImGui::IsKeyPressed(ImGuiKey_UpArrow))
                state.search_selection = (state.search_selection + static_cast<int>(results.size()) - 1) % static_cast<int>(results.size());
            if (submitted) open_search_result(results[static_cast<std::size_t>(state.search_selection)]);
        }
    }

    void render_profile(const ImVec2& position, float width)
    {
        const double now = ImGui::GetTime();
        if (now < state.profile_refreshed_at || now - state.profile_refreshed_at >= 1.0)
        {
            modules::player_esp::refresh_local_skin_texture();
            state.profile_label = "OneStep";
            state.profile_refreshed_at = now;
        }
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImVec2 head(position.x, position.y + 2.f), maximum(head.x + 42.f, head.y + 42.f);
        if (const auto texture = modules::player_esp::local_skin_texture_id())
        {
            const auto skin = reinterpret_cast<ImTextureID>(texture);
            draw->AddImageRounded(skin, head, maximum, ImVec2(8.f / 64.f, 8.f / 64.f),
                ImVec2(16.f / 64.f, 16.f / 64.f), IM_COL32_WHITE, 6.f);
            draw->AddImageRounded(skin, head, maximum, ImVec2(40.f / 64.f, 8.f / 64.f),
                ImVec2(48.f / 64.f, 16.f / 64.f), IM_COL32_WHITE, 6.f);
        }
        else
        {
            // Offline/loading fallback; replaced by the actual local skin as soon as it resolves.
            constexpr float pixel = 42.f / 8.f;
            draw->AddRectFilled(head, maximum, IM_COL32(169, 121, 90, 255), 6.f);
            draw->AddRectFilled(head, ImVec2(maximum.x, head.y + pixel * 2.5f),
                IM_COL32(61, 42, 30, 255), 6.f, ImDrawFlags_RoundCornersTop);
            auto block = [&](float x, float y, float w, float h, ImU32 color)
            {
                draw->AddRectFilled(ImVec2(head.x + x * pixel, head.y + y * pixel),
                    ImVec2(head.x + (x + w) * pixel, head.y + (y + h) * pixel), color);
            };
            block(1, 3, 2, 1, IM_COL32(224, 218, 209, 255));
            block(5, 3, 2, 1, IM_COL32(224, 218, 209, 255));
            block(2, 3, 1, 1, IM_COL32(83, 76, 151, 255));
            block(5, 3, 1, 1, IM_COL32(83, 76, 151, 255));
            block(3, 4, 2, 2, IM_COL32(136, 86, 61, 255));
            block(2, 6, 4, 1, IM_COL32(73, 46, 33, 255));
        }
        std::string player = friends::local_player_name();
        if (player.empty()) player = "Player";
        const ImVec4 clip(position.x + 53.f, position.y, position.x + width, position.y + 46.f);
        draw->AddText(gui::ui_bold_font ? gui::ui_bold_font : ImGui::GetFont(), 14.f,
            ImVec2(position.x + 53.f, position.y + 4.f), primary, player.c_str(), nullptr, 0, &clip);
        draw->AddText(gui::ui_font ? gui::ui_font : ImGui::GetFont(), 12.f,
            ImVec2(position.x + 53.f, position.y + 26.f), muted, state.profile_label.c_str(), nullptr, 0, &clip);
    }

    void render_module_options(modules::module* module, const ImVec2& position, float width, float height, float opacity)
    {
        ImGui::SetCursorScreenPos(position);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 4.f));
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 4.f));
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * opacity);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
        const ImVec4 control_accent = gui::get_dynamic_color().Value;
        ImGui::PushStyleColor(ImGuiCol_CheckMark, control_accent);
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, control_accent);
        ImGui::PushID(module->get_id());
        if (ImGui::BeginChild("##module_options", ImVec2(width, height),
            ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollbar))
        {
            ImGui::BeginDisabled(state.selected != module);
            const ImVec2 content_top = ImGui::GetCursorScreenPos();
            gui::option_toggle("Enabled", &module->enabled, false);
            const ImVec2 top = ImGui::GetCursorScreenPos();
            draw_text(ImGui::GetWindowDrawList(), gui::ui_font, 13.f,
                ImVec2(top.x + 10.f, top.y + 8.f), ImGui::GetColorU32(ImGuiCol_Text), "Keybind");
            const float binding_width = std::clamp(width - 120.f, 88.f, 132.f);
            ImGui::SetCursorScreenPos(ImVec2(top.x + width - 10.f - binding_width, top.y));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1, 1, 1, .045f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, .085f));
            module->key_bind_selector(binding_width);
            ImGui::PopStyleColor(2);
            ImGui::GetWindowDrawList()->AddLine(ImVec2(top.x + 10.f, top.y + 34.f),
                ImVec2(top.x + width - 10.f, top.y + 34.f), IM_COL32(255, 255, 255, 12));
            ImGui::SetCursorScreenPos(ImVec2(top.x, top.y + 42.f));
            ImGui::PushItemWidth(-8.f);
            module->render_options();
            ImGui::PopItemWidth();
            const ImGuiWindow* options_window = ImGui::GetCurrentWindow();
            // Measure all rows, including clipped and conditionally visible settings.
            state.options_content_height[module] = std::max(0.f,
                std::max(options_window->DC.CursorMaxPos.y,
                    options_window->DC.CursorPos.y - ImGui::GetStyle().ItemSpacing.y) - content_top.y);
            ImGui::EndDisabled();
        }
        ImGui::EndChild();
        ImGui::PopID();
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(4);
    }

    void render_module_settings(const ImVec2& position, float maximum_height, float width)
    {
        auto* module = state.settings_module;
        if (!module || state.settings_animation < .005f) return;
        const float eased = state.settings_animation * state.settings_animation * (3.f - 2.f * state.settings_animation);
        const auto measured = state.options_content_height.find(module);
        const float content_height = measured != state.options_content_height.end() ? measured->second : module->get_options_height() + 38.f;
        const auto* viewport = ImGui::GetMainViewport();
        const float limit = std::max(120.f, std::min(maximum_height, viewport->WorkPos.y + viewport->WorkSize.y - position.y - 12.f));
        const float height = 58.f + (std::clamp(content_height + 72.f, 120.f, limit) - 58.f) * eased;
        ImGui::SetNextWindowPos(position, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
        ImGui::Begin("##grim_settings_panel", nullptr, ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoFocusOnAppearing);
        // A right-side overlay must stay above the grid, but never above an open popup.
        if (!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
            ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
        auto* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(position, ImVec2(position.x + width, position.y + height), alpha(IM_COL32(16, 15, 21, 248), eased), 12.f);
        draw->AddRect(position, ImVec2(position.x + width, position.y + height), alpha(IM_COL32(255, 255, 255, 20), eased), 12.f);
        const auto title = title_case(module->get_name());
        const ImVec4 clip(position.x + 16.f, position.y + 12.f, position.x + width - 44.f, position.y + 44.f);
        draw->AddText(gui::ui_bold_font ? gui::ui_bold_font : ImGui::GetFont(), 15.f,
            ImVec2(position.x + 16.f, position.y + 18.f), alpha(primary, eased), title.c_str(), nullptr, 0.f, &clip);
        draw->AddLine(ImVec2(position.x + 16.f, position.y + 50.f), ImVec2(position.x + width - 16.f, position.y + 50.f), alpha(IM_COL32(255, 255, 255, 14), eased));
        ImGui::PushID(module->get_id());
        ImGui::SetCursorScreenPos(ImVec2(position.x + width - 36.f, position.y + 14.f));
        if (ImGui::InvisibleButton("##close_settings", ImVec2(24, 24))) state.selected = nullptr;
        const ImVec2 close(position.x + width - 24.f, position.y + 26.f);
        if (ImGui::IsItemHovered()) draw->AddCircleFilled(close, 11.f, alpha(IM_COL32(255, 255, 255, 14), eased));
        gui::control_draw::close_icon(draw, close, alpha(ImGui::IsItemHovered() ? primary : secondary, eased));
        if (height > 67.f)
            render_module_options(module, ImVec2(position.x + 6.f, position.y + 58.f), width - 12.f, height - 66.f, eased);
        ImGui::PopID();
        ImGui::End();
        ImGui::PopStyleVar(2);
    }

    bool player_preview_requested()
    {
        return state.current == page::visuals && state.selected &&
            std::strcmp(state.selected->get_id(), "player_esp") == 0;
    }

    void render_player_preview(const ImVec2& position)
    {
        // The sidebar profile refreshes the same local skin once per second.
        constexpr ImVec2 size(260.0f, 460.0f);
        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(position, ImVec2(position.x + size.x, position.y + size.y),
            IM_COL32(3, 4, 7, 222), 16.0f);
        draw->AddRect(position, ImVec2(position.x + size.x, position.y + size.y),
            IM_COL32(255, 255, 255, 15), 16.0f);

        const ImVec2 header_max(position.x + size.x, position.y + 52.0f);
        draw->AddRectFilled(position, header_max, IM_COL32(255, 255, 255, 5), 16.0f,
            ImDrawFlags_RoundCornersTop);
        icon(draw, "\xEF\x80\x87", ImVec2(position.x + 34.0f, position.y + 24.0f), accent_hover, 16.0f);
        draw_text(draw, gui::ui_bold_font, 16.5f, ImVec2(position.x + 54.0f, position.y + 13.0f), primary, "Player Preview");
        draw_text(draw, gui::ui_bold_font, 11.5f, ImVec2(position.x + 211.0f, position.y + 17.0f), accent_hover, "LIVE");

        const ImVec2 stage_min(position.x + 18.0f, position.y + 67.0f);
        const ImVec2 stage_max(position.x + 242.0f, position.y + 350.0f);
        draw->AddRectFilled(stage_min, stage_max, IM_COL32(1, 2, 4, 202), 12.0f);
        draw->AddRect(stage_min, stage_max, IM_COL32(255, 255, 255, 12), 12.0f);
        const ImVec2 box_min(position.x + 68.0f, position.y + 88.0f);
        const ImVec2 box_max(position.x + 192.0f, position.y + 332.0f);

        const auto settings = static_cast<modules::player_esp*>(state.selected)->get_preview_settings();
        ImGui::PushID("player_preview_stage");
        ImGui::SetCursorScreenPos(stage_min);
        ImGui::InvisibleButton("##rotate", ImVec2(stage_max.x - stage_min.x, stage_max.y - stage_min.y));
        const bool stage_hovered = ImGui::IsItemHovered();
        if (settings.mode == 2 && ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            state.player_preview_yaw += ImGui::GetIO().MouseDelta.x * 0.018f;
        if (settings.mode == 2 && stage_hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            state.player_preview_yaw = 0.0f;
        ImGui::PopID();

        ImVec4 preview_color(settings.color[0], settings.color[1], settings.color[2], settings.color[3]);
        if (settings.theme_color)
            preview_color = gui::get_dynamic_color().Value;
        const ImU32 esp_color = ImGui::ColorConvertFloat4ToU32(preview_color);

        if (settings.mode == 2)
        {
            ImVec4 fill = preview_color;
            fill.w = std::clamp(settings.fill_opacity, 0.03f, 0.45f);
            draw->AddRectFilled(box_min, box_max, ImGui::ColorConvertFloat4ToU32(fill), 5.0f);
        }

        const std::uintptr_t skin_texture_value = modules::player_esp::local_skin_texture_id();
        if (skin_texture_value)
        {
            const ImTextureID skin = reinterpret_cast<ImTextureID>(skin_texture_value);
            struct face_uv
            {
                float u0;
                float v0;
                float u1;
                float v1;
            };
            float normalized_yaw = settings.mode == 2 ? std::fmod(state.player_preview_yaw, 6.28318530718f) : 0.0f;
            if (normalized_yaw < 0.0f) normalized_yaw += 6.28318530718f;
            const int direction = static_cast<int>(std::floor((normalized_yaw + 0.78539816339f) / 1.57079632679f)) % 4;
            const float model_scale_x = direction == 1 || direction == 3 ? 0.70f : 1.0f;
            auto skin_part = [&](const ImVec2& minimum, const ImVec2& maximum,
                const std::array<face_uv, 4>& faces)
            {
                const float model_center = position.x + 130.0f;
                const float part_center = model_center + ((minimum.x + maximum.x) * 0.5f - model_center) * model_scale_x;
                const float half_width = (maximum.x - minimum.x) * 0.5f * model_scale_x;
                const ImVec2 scaled_min(part_center - half_width, minimum.y);
                const ImVec2 scaled_max(part_center + half_width, maximum.y);
                const face_uv& uv = faces[static_cast<std::size_t>(direction)];
                draw->AddImage(skin, scaled_min, scaled_max,
                    ImVec2(uv.u0 / 64.0f, uv.v0 / 64.0f), ImVec2(uv.u1 / 64.0f, uv.v1 / 64.0f));
            };
            const std::array<face_uv, 4> head{{ { 8, 8, 16, 16 }, { 0, 8, 8, 16 }, { 24, 8, 32, 16 }, { 16, 8, 24, 16 } }};
            const std::array<face_uv, 4> hat{{ { 40, 8, 48, 16 }, { 32, 8, 40, 16 }, { 56, 8, 64, 16 }, { 48, 8, 56, 16 } }};
            const std::array<face_uv, 4> torso{{ { 20, 20, 28, 32 }, { 16, 20, 20, 32 }, { 32, 20, 40, 32 }, { 28, 20, 32, 32 } }};
            const std::array<face_uv, 4> right_arm{{ { 44, 20, 48, 32 }, { 40, 20, 44, 32 }, { 52, 20, 56, 32 }, { 48, 20, 52, 32 } }};
            const std::array<face_uv, 4> left_arm{{ { 36, 52, 40, 64 }, { 32, 52, 36, 64 }, { 44, 52, 48, 64 }, { 40, 52, 44, 64 } }};
            const std::array<face_uv, 4> right_leg{{ { 4, 20, 8, 32 }, { 0, 20, 4, 32 }, { 12, 20, 16, 32 }, { 8, 20, 12, 32 } }};
            const std::array<face_uv, 4> left_leg{{ { 20, 52, 24, 64 }, { 16, 52, 20, 64 }, { 28, 52, 32, 64 }, { 24, 52, 28, 64 } }};
            skin_part(ImVec2(position.x + 100.0f, position.y + 101.0f), ImVec2(position.x + 160.0f, position.y + 161.0f), head);
            skin_part(ImVec2(position.x + 100.0f, position.y + 101.0f), ImVec2(position.x + 160.0f, position.y + 161.0f), hat);
            skin_part(ImVec2(position.x + 94.0f, position.y + 165.0f), ImVec2(position.x + 166.0f, position.y + 261.0f), torso);
            skin_part(ImVec2(position.x + 70.0f, position.y + 165.0f), ImVec2(position.x + 94.0f, position.y + 261.0f), right_arm);
            skin_part(ImVec2(position.x + 166.0f, position.y + 165.0f), ImVec2(position.x + 190.0f, position.y + 261.0f), left_arm);
            skin_part(ImVec2(position.x + 98.0f, position.y + 261.0f), ImVec2(position.x + 128.0f, position.y + 328.0f), right_leg);
            skin_part(ImVec2(position.x + 132.0f, position.y + 261.0f), ImVec2(position.x + 162.0f, position.y + 328.0f), left_leg);
        }
        else
        {
            const ImU32 body = alpha(blend(IM_COL32(28, 30, 38, 255), accent, 0.58f), 0.88f);
            const ImU32 body_dark = alpha(blend(IM_COL32(20, 22, 28, 255), accent, 0.35f), 0.92f);
            draw->AddRectFilled(ImVec2(position.x + 100.0f, position.y + 101.0f), ImVec2(position.x + 160.0f, position.y + 161.0f), body, 2.0f);
            draw->AddRectFilled(ImVec2(position.x + 94.0f, position.y + 165.0f), ImVec2(position.x + 166.0f, position.y + 261.0f), body, 2.0f);
            draw->AddRectFilled(ImVec2(position.x + 70.0f, position.y + 165.0f), ImVec2(position.x + 94.0f, position.y + 261.0f), body_dark, 2.0f);
            draw->AddRectFilled(ImVec2(position.x + 166.0f, position.y + 165.0f), ImVec2(position.x + 190.0f, position.y + 261.0f), body_dark, 2.0f);
            draw->AddRectFilled(ImVec2(position.x + 98.0f, position.y + 261.0f), ImVec2(position.x + 128.0f, position.y + 328.0f), body_dark, 2.0f);
            draw->AddRectFilled(ImVec2(position.x + 132.0f, position.y + 261.0f), ImVec2(position.x + 162.0f, position.y + 328.0f), body_dark, 2.0f);
        }
        if (settings.mode == 0)
            draw->AddRect(box_min, box_max, esp_color, 4.0f, 0, std::max(1.0f, settings.thickness));
        else if (settings.mode == 1)
        {
            const float length = 24.0f;
            const float thickness = std::max(1.0f, settings.thickness);
            draw->AddLine(box_min, ImVec2(box_min.x + length, box_min.y), esp_color, thickness);
            draw->AddLine(box_min, ImVec2(box_min.x, box_min.y + length), esp_color, thickness);
            draw->AddLine(ImVec2(box_max.x, box_min.y), ImVec2(box_max.x - length, box_min.y), esp_color, thickness);
            draw->AddLine(ImVec2(box_max.x, box_min.y), ImVec2(box_max.x, box_min.y + length), esp_color, thickness);
            draw->AddLine(ImVec2(box_min.x, box_max.y), ImVec2(box_min.x + length, box_max.y), esp_color, thickness);
            draw->AddLine(ImVec2(box_min.x, box_max.y), ImVec2(box_min.x, box_max.y - length), esp_color, thickness);
            draw->AddLine(box_max, ImVec2(box_max.x - length, box_max.y), esp_color, thickness);
            draw->AddLine(box_max, ImVec2(box_max.x, box_max.y - length), esp_color, thickness);
        }
        else
        {
            draw->AddRect(box_min, box_max, esp_color, 4.0f, 0, std::max(1.0f, settings.thickness));
            draw->AddLine(ImVec2(box_min.x, box_min.y), ImVec2(box_min.x + 14.0f, box_min.y - 8.0f), esp_color, settings.thickness);
            draw->AddLine(ImVec2(box_max.x, box_min.y), ImVec2(box_max.x + 14.0f, box_min.y - 8.0f), esp_color, settings.thickness);
        }
        if (settings.health)
        {
            draw->AddRectFilled(ImVec2(position.x + 57.0f, position.y + 88.0f), ImVec2(position.x + 62.0f, position.y + 332.0f), IM_COL32(16, 18, 24, 205), 2.5f);
            draw->AddRectFilled(ImVec2(position.x + 58.0f, position.y + 137.0f), ImVec2(position.x + 61.0f, position.y + 331.0f), IM_COL32(78, 230, 132, 255), 2.0f);
        }
        if (settings.names)
        {
            const char* player_name = "Player";
            const ImVec2 extent = (gui::ui_bold_font ? gui::ui_bold_font : ImGui::GetFont())->CalcTextSizeA(gui::typography::caption, FLT_MAX, 0.0f, player_name);
            draw_text(draw, gui::ui_bold_font, 13.0f, ImVec2(position.x + 130.0f - extent.x * 0.5f, position.y + 72.0f), primary, player_name);
        }
        if (settings.distance)
            draw_text(draw, gui::ui_bold_font, 12.0f, ImVec2(position.x + 164.0f, position.y + 335.0f), muted, "4.2m");
        if (settings.mode == 2 && stage_hovered)
        {
            const char* hint = "Drag to rotate";
            ImFont* hint_font = gui::ui_bold_font ? gui::ui_bold_font : ImGui::GetFont();
            const ImVec2 extent = hint_font->CalcTextSizeA(gui::typography::caption, FLT_MAX, 0.0f, hint);
            draw->AddRectFilled(ImVec2(position.x + 130.0f - extent.x * 0.5f - 7.0f, position.y + 315.0f),
                ImVec2(position.x + 130.0f + extent.x * 0.5f + 7.0f, position.y + 337.0f), IM_COL32(4, 4, 7, 188), 6.0f);
            draw_text(draw, hint_font, 10.5f,
                ImVec2(position.x + 130.0f - extent.x * 0.5f, position.y + 320.0f), secondary, hint);
        }

        struct metric { const char* name; const char* value; };
        const metric metrics[] = {
            { "Style", settings.mode == 0 ? "2D Box" : (settings.mode == 1 ? "Corners" : "3D Box") },
            { "Health", settings.health ? "Visible" : "Hidden" },
            { "Distance", settings.distance ? "Visible" : "Hidden" }
        };
        float metric_x = position.x + 18.0f;
        for (const metric& value : metrics)
        {
            const ImVec2 chip(metric_x, position.y + 365.0f);
            draw->AddRectFilled(chip, ImVec2(metric_x + 69.0f, position.y + 415.0f), IM_COL32(13, 14, 19, 210), 8.0f);
            draw_text(draw, gui::ui_bold_font, 10.5f, ImVec2(chip.x + 8.0f, chip.y + 8.0f), muted, value.name);
            draw_text(draw, gui::ui_bold_font, 12.0f, ImVec2(chip.x + 8.0f, chip.y + 26.0f), primary, value.value);
            metric_x += 77.0f;
        }
        char thickness[48]{};
        std::snprintf(thickness, sizeof(thickness), "Thickness %.1f  /  Alpha %d%%", settings.thickness,
            static_cast<int>(preview_color.w * 100.0f));
        draw_text(draw, gui::ui_bold_font, 12.0f, ImVec2(position.x + 20.0f, position.y + 432.0f), muted, thickness);
    }
}

void gui::grim::reset_runtime_state()
{
    state = {};
    gui::keybinds_page::reset();
}

void gui::grim::render(const ImGuiViewport* viewport, const page_callbacks& callbacks)
{
    // A minimized window has no useful layout area. Do not clamp the saved
    // anchor into a zero-sized viewport and strand it at the edge on return.
    if (!viewport || viewport->WorkSize.x < 1.f || viewport->WorkSize.y < 1.f) {
        state.dragging_shell = false;
        return;
    }
    if (state.rendered_frame != ImGui::GetFrameCount() - 1) state.dragging_shell = false;
    const bool escape_pressed = ImGui::IsKeyPressed(ImGuiKey_Escape, false);
    const bool search_was_active = state.search_active || state.search[0];
    constexpr auto popup_flags = ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel;
    const bool popup_was_open = ImGui::IsPopupOpen(nullptr, popup_flags) ||
        (state.rendered_frame == ImGui::GetFrameCount() - 1 && state.had_popup);
    const bool capturing_key = std::any_of(modules::get_modules().begin(), modules::get_modules().end(),
        [](const modules::module* module) { return module->is_keybind_capture_active(); });
    // Keyboard navigation is intentionally disabled in the game integration,
    // so dismiss ordinary ImGui popups explicitly. Modal confirmations own
    // their cancellation state and are handled by their page instead.
    if (escape_pressed && !capturing_key &&
        ImGui::IsPopupOpen(nullptr, popup_flags) && !ImGui::GetTopMostPopupModal())
        ImGui::ClosePopupToLevel(ImGui::GetCurrentContext()->OpenPopupStack.Size - 1, true);
    if (escape_pressed && search_was_active && !popup_was_open && !capturing_key) {
        clear_search();
        state.search_active = false;
        state.focus_search = false;
        ImGui::ClearActiveID();
    }
    gui::background::update();
    accent = gui::get_dynamic_color();
    accent_hover = blend(accent, primary, 0.16f);
    if (gui::hud_layout_editor_active)
    {
        gui::hud_editor_active = true;
        gui::hud_editor::render(viewport);
        return;
    }
    gui::hud_editor_active = false;
    ImGui::GetBackgroundDrawList()->AddRectFilled(viewport->Pos,
        ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y), IM_COL32(0, 0, 0, 32));
    const bool module_page = state.search[0] || std::any_of(std::begin(feature_navigation), std::end(feature_navigation),
        [](const navigation_entry& entry) { return state.current == entry.target; });
    constexpr float sidebar_width = 204.f;
    constexpr float preview_gap = 16.f;
    constexpr float preview_width = 260.f;
    if (!module_page) {
        state.settings_module = nullptr;
        state.settings_animation = 0.f;
    } else {
        const bool requested = has_settings_panel(state.selected);
        if (requested) state.settings_module = state.selected;
        state.settings_animation = approach(state.settings_animation, requested ? 1.f : 0.f, 12.f);
        if (!requested && state.settings_animation < .005f) state.settings_module = nullptr;
    }
    const dock_layout::box work{viewport->WorkPos.x, viewport->WorkPos.y, viewport->WorkSize.x, viewport->WorkSize.y};
    const auto anchored = dock_layout::main_panel(work);
    const ImVec2 main_size(anchored.width, anchored.height);
    if (!state.positioned)
    {
        state.position = ImVec2(anchored.x, anchored.y);
        state.positioned = true;
    }
    // All navigation/search modes share the same bounded, absolute anchor.
    // Drawers and the optional player preview never participate in positioning.
    const auto main_box = dock_layout::positioned_main(work, state.position.x, state.position.y);
    const ImVec2 menu_position(main_box.x, main_box.y);
    state.position = menu_position;
    const auto settings_box = dock_layout::settings_panel(work, main_box);
    const bool preview_requested = module_page && player_preview_requested() && dock_layout::preview_fits(work, main_box, preview_width);
    const float left_extra = preview_requested ? preview_width + preview_gap : 0.f;
    const ImVec2 root_position(menu_position.x - left_extra, menu_position.y);
    ImGui::SetNextWindowPos(root_position, ImGuiCond_Always);
    // The settings panel is its own input window. Extending this invisible
    // root over it would intercept hover/wheel events after a card takes focus.
    ImGui::SetNextWindowSize(ImVec2(left_extra + main_size.x, main_size.y), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    if (ImGui::Begin("##ThedoriusGrimUI", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus))
    {
        const ImVec2 root_origin = ImGui::GetWindowPos();
        const ImVec2 origin(root_origin.x + left_extra, root_origin.y);
        ImDrawList* draw = ImGui::GetWindowDrawList();
        if (preview_requested)
            render_player_preview(root_origin);
        const auto menu_surface = [&](const ImVec2& minimum, const ImVec2& size) {
            const ImVec2 maximum(minimum.x + size.x, minimum.y + size.y);
            for (const auto& layer : std::array<std::pair<float, int>, 3>{{{16.f, 10}, {8.f, 18}, {3.f, 28}}})
                ImGui::GetBackgroundDrawList()->AddRectFilled(
                    ImVec2(minimum.x - layer.first, minimum.y - layer.first),
                    ImVec2(maximum.x + layer.first, maximum.y + layer.first), IM_COL32(0, 0, 0, layer.second), 20.f + layer.first);
            draw->AddRectFilled(minimum, maximum, shell, 20.f);
            gui::background::render(draw, minimum, size, gui::ui_state::get_preferences().menu_background);
            draw->AddRect(minimum, maximum, IM_COL32(255, 255, 255, 12), 20.f);
        };
        menu_surface(origin, main_size);
        draw->AddRectFilled(origin, ImVec2(origin.x + sidebar_width, origin.y + main_size.y), sidebar, 20.0f, ImDrawFlags_RoundCornersLeft);
        draw->AddRectFilledMultiColor(ImVec2(origin.x + 1.0f, origin.y + 1.0f),
            ImVec2(origin.x + sidebar_width - 1.0f, origin.y + 82.0f),
            alpha(accent, 7.0f / 255.0f), alpha(accent, 2.0f / 255.0f),
            alpha(accent, 0.0f), alpha(accent, 0.0f));

        ImGui::SetCursorScreenPos(ImVec2(origin.x + 8.0f, origin.y + 7.0f));
        ImGui::InvisibleButton("##drag_shell", ImVec2(sidebar_width - 16.0f, 47.0f));
        const bool valid_pointer = !ImGui::GetIO().AppFocusLost && ImGui::IsMousePosValid();
        if (ImGui::IsItemActivated() && valid_pointer) {
            const auto mouse = ImGui::GetMousePos();
            state.drag_grab_offset = ImVec2(mouse.x - menu_position.x, mouse.y - menu_position.y);
            state.dragging_shell = true;
        }
        if (!ImGui::IsItemActive() || !valid_pointer || !ImGui::IsMouseDown(ImGuiMouseButton_Left))
            state.dragging_shell = false;
        if (state.dragging_shell && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            const auto mouse = ImGui::GetMousePos();
            const auto placed = dock_layout::positioned_main(work,
                mouse.x - state.drag_grab_offset.x, mouse.y - state.drag_grab_offset.y);
            state.position = ImVec2(placed.x, placed.y);
        }

        if (const ImTextureID helmet = gui::get_icon_texture(gui::icon_asset::helmet))
            draw->AddImage(helmet, ImVec2(origin.x + 16.0f, origin.y + 14.0f), ImVec2(origin.x + 52.0f, origin.y + 50.0f));
        const float logo_sz = 19.0f;
        const ImVec2 one_sz = (gui::ui_bold_font ? gui::ui_bold_font : ImGui::GetFont())->CalcTextSizeA(logo_sz, FLT_MAX, 0.0f, "one");
        draw_text(draw, gui::ui_bold_font, logo_sz, ImVec2(origin.x + 60.0f, origin.y + 14.0f), IM_COL32(245, 245, 248, 255), "one");
        draw_text(draw, gui::ui_bold_font, logo_sz, ImVec2(origin.x + 60.0f + one_sz.x, origin.y + 14.0f), gui::get_dynamic_color(0), "step");
        draw_text(draw, gui::ui_bold_font, 12.0f, ImVec2(origin.x + 60.0f, origin.y + 38.0f), muted, "v1.0");

        const float nav_x = origin.x + 14.0f;
        draw_text(draw, gui::ui_bold_font, 10.5f, ImVec2(nav_x + 4.0f, origin.y + 73.0f), muted, "FEATURES");
        float nav_y = origin.y + 89.0f;
        for (const navigation_entry& entry : feature_navigation)
        {
            navigation_button(entry, ImVec2(nav_x, nav_y), 176.0f);
            nav_y += 40.0f;
        }
        nav_y += 5.0f;
        draw_text(draw, gui::ui_bold_font, 10.5f, ImVec2(nav_x + 4.0f, nav_y), muted, "OTHER");
        nav_y += 17.0f;
        for (const navigation_entry& entry : other_navigation)
        {
            navigation_button(entry, ImVec2(nav_x, nav_y), 176.0f);
            nav_y += 40.0f;
        }

        const ImVec2 profile(origin.x + 14.0f, origin.y + main_size.y - 58.0f);
        render_profile(profile, sidebar_width - 28.f);

        render_search(ImVec2(origin.x + 14.f, profile.y - 48.f), 176.f);

        const ImVec2 content(origin.x + sidebar_width + 18.0f, origin.y + 13.0f);
        const float content_width = main_size.x - sidebar_width - 36.0f;
        const bool searching = state.search[0] != '\0';
        const ImVec2 heading(content.x + (searching ? 12.f : 0.f), content.y + (searching ? 8.f : 0.f));
        const char* page_title = searching ? "Search results" : page_label(state.current);
        const navigation_entry* active_entry = nullptr;
        for (const navigation_entry& entry : feature_navigation) if (entry.target == state.current) active_entry = &entry;
        for (const navigation_entry& entry : other_navigation) if (entry.target == state.current) active_entry = &entry;
        if (searching) {
            const ImVec2 center(heading.x + 8.f, heading.y + 9.f);
            draw->AddCircle(center, 6.f, accent_hover, 0, 1.7f);
            draw->AddLine(ImVec2(center.x + 4, center.y + 4), ImVec2(center.x + 9, center.y + 9), accent_hover, 1.7f);
        } else if (active_entry) icon(draw, active_entry->glyph, ImVec2(content.x + 9.0f, content.y + 12.0f), accent_hover, 16.0f);
        draw_text(draw, gui::ui_bold_font, 19.0f, ImVec2(heading.x + 28.0f, heading.y), primary, page_title);
        draw_text(draw, gui::ui_bold_font, 13.0f, ImVec2(content.x + 28.0f, content.y + 26.0f), muted, searching ? "" : page_description(state.current));

        const ImVec2 body(content.x, content.y + 57.0f);
        const ImVec2 body_size(content_width, main_size.y - 77.0f);
        state.page_fade = approach(state.page_fade, 1.0f, 14.0f);
        const float page_alpha = state.page_fade * state.page_fade * (3.0f - 2.0f * state.page_fade);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, std::clamp(page_alpha, 0.02f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        if (searching)
        {
            render_grid(body, body_size);
        }
        else if (state.current == page::friends && callbacks.friends)
        {
            ImGui::SetCursorScreenPos(body);
            if (ImGui::BeginChild("##friends_page", body_size, ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar)) callbacks.friends();
            ImGui::EndChild();
        }
        else if (state.current == page::configs && callbacks.configs)
        {
            ImGui::SetCursorScreenPos(body);
            if (ImGui::BeginChild("##configs_page", body_size, ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar)) callbacks.configs();
            ImGui::EndChild();
        }
        else if (state.current == page::hud)
        {
            render_hud_editor_launcher(body, body_size);
        }
        else if (state.current == page::themes)
        {
            ImGui::SetCursorScreenPos(body);
            if (ImGui::BeginChild("##themes_page", body_size, ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar)) gui::render_theme_selection();
            ImGui::EndChild();
        }
        else if (state.current == page::keybinds)
        {
            ImGui::SetCursorScreenPos(body);
            if (ImGui::BeginChild("##keybinds_page", body_size, ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar)) gui::keybinds_page::render();
            ImGui::EndChild();
        }
        else
        {
            render_grid(body, body_size);
        }
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        if (module_page && (searching || (state.current != page::friends && state.current != page::configs &&
            state.current != page::themes && state.current != page::hud && state.current != page::keybinds)))
            render_module_settings(ImVec2(settings_box.x, settings_box.y), main_size.y, settings_box.width);
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    const bool popup_open = ImGui::IsPopupOpen(nullptr, popup_flags);
    if (escape_pressed && !search_was_active && !popup_was_open && !popup_open && !capturing_key)
    {
        gui::suppress_escape_until_release.store(true, std::memory_order_release);
        gui::draw = false;
        ImGui::ClearActiveID();
    }
    state.had_popup = popup_open;
    state.rendered_frame = ImGui::GetFrameCount();
}
