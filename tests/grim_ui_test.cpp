// Include the shipping implementation to exercise private input/layout state
// without adding any test-only switches or entry points to the DLL.
#include "../src/ui/grim_ui.cpp"
#include "../src/ui/settings_renderer.hpp"
#include "../src/gui/surface_style.hpp"
#include "../src/modules/combat/legit_aim_config.hpp"
#include "../src/ui/keybinds_model.hpp"
#include "../src/configs/keybind_profile.hpp"
#include "../src/modules/core/public_config.hpp"
#include "../src/modules/render/arraylist.hpp"
#include "../src/modules/visual/spotify_hud.hpp"
#include "support/imgui_capture.hpp"
#include <cassert>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <memory>

namespace {
    std::vector<modules::module*> entries;
    gui::ui_state::preferences preferences;
    int last_key = 0, configs_rendered = 0, friends_rendered = 0;
    class fixture_module : public modules::module {
    public:
        using module::module;
        legit_aim_engine::config settings;
        int extra_rows = 0;
        float get_options_height() override { return 280.f; }
        void render_options() override {
            gui::settings::render(settings, legit_aim_engine::schema());
            for (int i = 0; i < extra_rows; ++i) {
                ImGui::PushID(i); gui::option_toggle("Extra test control", &settings.weapon_only); ImGui::PopID();
            }
        }
    };
    class fixed_shortcut : public modules::module {
    public:
        fixed_shortcut() : module("fixed shortcut", "System", "fixed_shortcut") {}
        bool supports_keybind() const override { return false; }
    };
    class information_only : public modules::module {
    public:
        information_only() : module("information only", "Visual", "information_only") {}
        float get_options_height() override { return 120.f; }
        bool has_custom_options() override { return false; }
        void render_options() override { assert(false); } // Must never open.
    };
}
// Only platform/game services are fixtures; actual GrimUI, widgets, module base,
// keybind UI, schema renderer, font atlas and Dear ImGui are compiled unchanged.
namespace modules {
    const std::vector<module*>& get_modules() { return entries; }
    module* find_module_by_id(const std::string& id) {
        for (auto* entry : entries) if (id == entry->get_id()) return entry;
        return nullptr;
    }
    std::uintptr_t player_esp::local_skin_texture_id() { return 0; }
    void player_esp::refresh_local_skin_texture() {}
    bool arraylist::draw_overlay(ImDrawList*, float, float, bool, overlay_bounds*) { return false; }
    // The real HUD editor dynamic_casts registry entries to these types. Supply
    // complete inert virtual tables/RTTI in this fixture, without linking game
    // overlays, Spotify services or platform hooks into the headless test.
    void spotify_hud::run(::cache&) {}
    void spotify_hud::render_overlay() {}
    void spotify_hud::render_options() {}
    void spotify_hud::on_enable(::cache&) {}
    void spotify_hud::on_disable(::cache&) {}
    spotify_hud::bounds spotify_hud::get_bounds(float, float) const { return {}; }
    void spotify_hud::render_hud_preview(ImDrawList*, float, float) {}

}
namespace friends { std::string local_player_name() { return "Player"; } }
namespace gui {
    ImColor get_dynamic_color(int) { return ImColor(155, 69, 255); }
    ImTextureID get_icon_texture(icon_asset asset) { return asset == icon_asset::helmet && !capture_helmet.rgba.empty()
        ? reinterpret_cast<ImTextureID>(2) : nullptr; }
    void render_theme_selection() { ImGui::TextUnformatted("Appearance fixture"); }
    void request_panic() { should_unload = true; draw = false; }
    int get_last_pressed_key() { return last_key; }
    void reset_last_pressed_key() { last_key = 0; }
}
namespace gui::ui_state {
    preferences& get_preferences() { return ::preferences; }
    void mark_preferences_dirty() {}
    void flush_preferences(bool, bool) {}
}
namespace gui::background {
    void update() {}
    bool render(ImDrawList*, ImVec2, ImVec2, const options&) { return false; }
}

namespace {
    void frame(int count = 1) {
        for (int i = 0; i < count; ++i) {
            ImGui::NewFrame();
            gui::grim::page_callbacks callbacks;
            callbacks.friends = [] { ++friends_rendered; ImGui::TextUnformatted("Friends fixture"); };
            callbacks.configs = [] { ++configs_rendered; ImGui::TextUnformatted("Configs fixture"); };
            gui::grim::render(ImGui::GetMainViewport(), callbacks);
            ImGui::Render();
        }
    }
    ImGuiWindow* grid() {
        auto* root = ImGui::FindWindowByName("##ThedoriusGrimUI");
        for (auto* child : root->DC.ChildWindows)
            if (std::strstr(child->Name, "##grim_module_grid")) return child;
        assert(false); return nullptr;
    }
    ImGuiWindow* settings_content() {
        auto* panel = ImGui::FindWindowByName("##grim_settings_panel");
        assert(panel);
        for (auto* child : panel->DC.ChildWindows) if (child->Active) return child;
        assert(false); return nullptr;
    }
    void click(ImVec2 p) {
        auto& io = ImGui::GetIO();
        io.AddMousePosEvent(p.x, p.y); frame(2);
        io.AddMouseButtonEvent(0, true); frame();
        io.AddMouseButtonEvent(0, false); frame(3);
    }
    void key(ImGuiKey value) {
        ImGui::GetIO().AddKeyEvent(value, true); frame();
        ImGui::GetIO().AddKeyEvent(value, false); frame(2);
    }
    void capture(const std::string& directory, const char* name) {
        if (!directory.empty()) capture_imgui(directory + '/' + name + ".ppm", 2.f);
    }
    ImGuiWindow* active_window(const char* token) {
        for (auto* window : ImGui::GetCurrentContext()->Windows)
            if (window->Active && std::strstr(window->Name,token)) return window;
        assert(false); return nullptr;
    }
    void select_keyboard_key(int code) {
        const auto* descriptor = platform::keys::find(code); assert(descriptor);
        const auto* surface = active_window("##keybinds_surface");
        const float unit = (surface->Size.x - 2.f * surface->WindowPadding.x) / 23.f;
        const auto origin = surface->DC.CursorStartPos;
        click({origin.x + (descriptor->x + descriptor->w * .5f) * unit,
            origin.y + ImGui::GetIO().FontDefault->FontSize + 6.f + (descriptor->y + descriptor->h * .5f) * unit});
    }
    void select_binding_row(int index, bool clear = false) {
        const auto* list = active_window("##module_selection");
        click({clear ? list->Pos.x + list->Size.x - 16.f : list->Pos.x + 60.f,
            list->DC.CursorStartPos.y + ImGui::GetIO().FontDefault->FontSize + 6.f + index * 38.f + 16.f});
    }
}
int main(int argc, char** argv)
{
    std::signal(SIGABRT, [](int) {
        if (GImGui && GImGui->CurrentWindow)
            std::cerr << "Active UI window on assertion: " << GImGui->CurrentWindow->Name << '\n';
        std::_Exit(EXIT_FAILURE);
    });
    const std::string capture_dir = argc > 1 ? argv[1] : "";
    if (!capture_dir.empty()) {
        std::ifstream source(capture_dir + "/helmet.rgba", std::ios::binary);
        if (source) {
            source.read(reinterpret_cast<char*>(&capture_helmet.width), sizeof(int));
            source.read(reinterpret_cast<char*>(&capture_helmet.height), sizeof(int));
            assert(capture_helmet.width > 0 && capture_helmet.width <= 2048 && capture_helmet.height > 0 && capture_helmet.height <= 2048);
            capture_helmet.rgba.resize(capture_helmet.width * capture_helmet.height * 4);
            source.read(reinterpret_cast<char*>(capture_helmet.rgba.data()), capture_helmet.rgba.size()); assert(source);
        }
    }
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.DisplaySize = {1600, 900}; io.DeltaTime = 1.f / 60.f; io.IniFilename = nullptr;
    ImFontConfig font_config;
    font_config.OversampleH = 3; font_config.OversampleV = 1;
    // Re-rasterize glyphs for the 2x capture; never enlarge an existing PNG.
    font_config.RasterizerDensity = capture_dir.empty() ? 1.f : 2.f;
    io.DisplayFramebufferScale = capture_dir.empty() ? ImVec2(1,1) : ImVec2(2,2);
    gui::ui_font = io.Fonts->AddFontFromFileTTF("assets/fonts/Montserrat-SemiBold.ttf", 19.5f, &font_config);
    gui::ui_bold_font = io.Fonts->AddFontFromFileTTF("assets/fonts/Montserrat-Bold.ttf", 22.5f, &font_config);
    static constexpr ImWchar icon_range[]{0xf000, 0xf8ff, 0};
    gui::icon_font = io.Fonts->AddFontFromFileTTF("assets/fonts/FontAwesome-Free-Solid-900.otf", 20.f, &font_config, icon_range);
    assert(gui::ui_font && gui::ui_bold_font && gui::icon_font);
    io.FontDefault = gui::ui_font;
    io.Fonts->Build(); io.Fonts->SetTexID(reinterpret_cast<ImTextureID>(1));
    auto& style = ImGui::GetStyle();
    gui::surface_style::configure(style);
    style.WindowPadding = {14,14}; style.ItemSpacing = {10,9}; style.FramePadding = {10,7};
    style.Colors[ImGuiCol_SliderGrabActive] = ImColor(155, 69, 255);
    style.Colors[ImGuiCol_Text] = ImColor(235, 235, 242);
    ::preferences.reduce_motion = true;
    gui::draw = true;
    std::vector<std::unique_ptr<fixture_module>> owned;
    for (const char* name : {"legit click aim assist", "blatant click aim assist", "aim assist", "triggerbot",
        "jump reset", "shield breaker"}) {
        owned.push_back(std::make_unique<fixture_module>(name)); entries.push_back(owned.back().get());
    }
    assert(argc > 2);
    const auto profile_path = std::filesystem::path(argv[2]);
    assert(configs::keybind_profile::initialize(profile_path));
    // Save and reopen a real local file using production config parsing. Only
    // bindings may change; startup loading must not enable gameplay modules.
    entries[0]->set_keybind(platform::keys::num_enter);
    assert(configs::keybind_profile::flush(true) && std::filesystem::exists(profile_path));
    entries[0]->set_keybind(0); entries[0]->enabled = true;
    assert(configs::keybind_profile::initialize(profile_path));
    assert(entries[0]->get_keybind() == platform::keys::num_enter && entries[0]->enabled);
    const auto saved_bindings = modules::export_keybind_config();
    std::string import_message;
    for (const char* invalid : {
        R"({"modules":[{"id":"legit_click_aim_assist","keybind":65},{"id":"triggerbot","keybind":256}]})",
        R"({"modules":[{"id":"legit_click_aim_assist","keybind":65},{"id":"legit_click_aim_assist","keybind":66}]})",
        R"({"modules":[{"id":"triggerbot","keybind":1.5}]})", R"({"modules":[null]})"}) {
        assert(!modules::apply_keybind_config(invalid,import_message));
        assert(modules::export_keybind_config() == saved_bindings);
    }
    // Public configs retain the new exact code and all historical VK values.
    const auto shared = modules::export_public_config();
    entries[0]->set_keybind(0);
    assert(modules::apply_public_config(shared,import_message));
    assert(entries[0]->get_keybind() == platform::keys::num_enter);
    modules::module no_options("plain module", "Custom group", "plain_module");
    fixed_shortcut fixed;
    entries.push_back(&no_options); entries.push_back(&fixed);
    assert(gui::keybinds_model::filtered("plain", "Custom group").size() == 1);
    assert(gui::keybinds_model::filtered("fixed shortcut", "").empty());
    assert(gui::keybinds_model::assign(no_options,'G') && !gui::keybinds_model::assign(fixed,'G'));
    assert(!gui::keybinds_model::assign(no_options,0xa1));
    auto groups = gui::keybinds_model::categories();
    assert(std::find(groups.begin(),groups.end(),"Custom group") != groups.end());
    {
        modules::module movement("movement fixture", "Movement"), player("player fixture", "Player"),
            world("world fixture", "World"), friends_page("friends fixture", "Friends");
        const auto previous_size = entries.size();
        entries.insert(entries.end(), {&movement, &player, &world, &friends_page});
        const auto normalized = gui::keybinds_model::categories();
        assert(std::count(normalized.begin(), normalized.end(), "Misc") == 1);
        for (const auto legacy : {"Friends", "Movement", "Player", "World"})
            assert(std::find(normalized.begin(), normalized.end(), legacy) == normalized.end());
        assert(gui::keybinds_model::filtered("fixture", "Misc").size() == 3);
        assert(gui::keybinds_model::filtered("friends fixture", "").empty());
        entries.resize(previous_size);
    }
    assert(configs::keybind_profile::flush(true));
    entries.pop_back(); entries.pop_back();
    // A failing path must be reported, not counted as a successful save.
    const auto blocked = profile_path.parent_path() / "blocked";
    { std::ofstream file(blocked); file << "not a directory"; }
    assert(!configs::keybind_profile::initialize(blocked / "keybinds.json"));
    entries[0]->set_keybind('A');
    assert(!configs::keybind_profile::flush(true) && !configs::keybind_profile::healthy());
    // Invalid existing profiles must neither partially apply nor be silently
    // overwritten on a frame with no user edits.
    const auto corrupt = profile_path.parent_path() / "corrupt.json";
    const std::string broken = R"({"modules":[{"id":"legit_click_aim_assist","keybind":66},{"id":"triggerbot","keybind":256}]})";
    { std::ofstream file(corrupt); file << broken; }
    const auto before_corrupt = modules::export_keybind_config();
    assert(!configs::keybind_profile::initialize(corrupt));
    assert(modules::export_keybind_config() == before_corrupt);
    assert(!configs::keybind_profile::flush(true));
    { std::ifstream file(corrupt); assert(std::string((std::istreambuf_iterator<char>(file)),{}) == broken); }
    // A destination directory causes atomic replacement to fail. Its contents
    // must survive, and the temporary file must not be left behind.
    const auto directory_target = profile_path.parent_path() / "directory.json";
    std::filesystem::create_directory(directory_target);
    { std::ofstream file(directory_target / "keep"); file << "keep"; }
    assert(!configs::keybind_profile::initialize(directory_target));
    entries[0]->set_keybind('B');
    assert(!configs::keybind_profile::flush(true));
    assert(std::filesystem::exists(directory_target / "keep"));
    assert(!std::filesystem::exists(directory_target.string() + ".tmp"));
    assert(configs::keybind_profile::initialize(profile_path));
    entries[0]->set_keybind(0); entries[0]->enabled = false;
    assert(configs::keybind_profile::flush(true));
    frame(3);
    assert(visible_modules() == entries);
    const float initial_width = grid()->Size.x;
    assert(initial_width > 700.f);
    assert(grid()->ContentSize.y < 600.f); // Readable, wrapped cards still occupy six rows, not eleven.
    capture(capture_dir, "combat-grid");
    {
        auto* shell_window = ImGui::FindWindowByName("##ThedoriusGrimUI");
        const auto initial_position = shell_window->Pos;
        const ImVec2 grab(initial_position.x + 80.f, initial_position.y + 26.f);
        io.AddMousePosEvent(grab.x, grab.y); frame(2);
        io.AddMouseButtonEvent(0, true); frame();
        io.AddMousePosEvent(grab.x + 72.f, grab.y + 40.f); frame(3);
        io.AddMouseButtonEvent(0, false); frame(3);
        const auto dragged = shell_window->Pos;
        assert(std::abs(dragged.x - initial_position.x - 72.f) < 1.f);
        assert(std::abs(dragged.y - initial_position.y - 40.f) < 1.f);
        for (const auto target : {page::friends, page::combat, page::configs, page::misc,
            page::themes, page::vape, page::hud, page::client, page::keybinds, page::combat}) {
            state.current = target; frame(3);
            assert(shell_window->Pos.x == dragged.x && shell_window->Pos.y == dragged.y);
        }
        state.current = page::configs; frame(3);
        std::strcpy(state.search, "aim"); frame(3);
        assert(shell_window->Pos.x == dragged.x && shell_window->Pos.y == dragged.y);
        clear_search(); frame(3);
        assert(shell_window->Pos.x == dragged.x && shell_window->Pos.y == dragged.y);
        io.DisplaySize = {0,0}; frame(2); // Minimized frames must not overwrite the anchor.
        assert(state.position.x == dragged.x && state.position.y == dragged.y);
        io.DisplaySize = {1600,900}; frame(3);
        assert(shell_window->Pos.x == dragged.x && shell_window->Pos.y == dragged.y);
        state.position = initial_position; state.current = page::combat; frame(3);
    }
    {
        // A long list must retain its scroll extent without tessellating every
        // offscreen card. Only local fixtures are added; no game work is run.
        const auto original_size = entries.size();
        const int initial_vertices = grid()->DrawList->VtxBuffer.Size;
        std::vector<std::unique_ptr<fixture_module>> offscreen;
        for (int i = 0; i < 160; ++i) {
            const auto id = "offscreen_fixture_" + std::to_string(i);
            offscreen.push_back(std::make_unique<fixture_module>("offscreen fixture", "Combat", id.c_str()));
            entries.push_back(offscreen.back().get());
        }
        frame(3);
        assert(visible_modules().size() == entries.size());
        assert(grid()->ScrollMax.y > 5000.f);
        assert(grid()->DrawList->VtxBuffer.Size < initial_vertices * 2);
        const auto layouts = state.card_layouts.size();
        frame(3);
        assert(state.card_layouts.size() == layouts);
        ImGui::SetScrollY(grid(), grid()->ScrollMax.y); frame(3);
        assert(grid()->Scroll.y > 5000.f && grid()->DrawList->VtxBuffer.Size > 0);
        assert(grid()->DrawList->VtxBuffer.Size < initial_vertices * 2);
        std::strcpy(state.search, "offscreen");
        assert(search_results().size() == 160);
        entries.resize(original_size);
        assert(search_results().empty()); // Registry changes invalidate cached results.
        clear_search(); frame(3);
        assert(visible_modules() == entries && grid()->Scroll.y == 0.f);
    }
    auto top = grid()->Pos;
    io.AddMousePosEvent(top.x + 35.f, top.y + 28.f); frame(3);
    for (const auto* window : GImGui->Windows)
        assert(!window->Active || !(window->Flags & ImGuiWindowFlags_Tooltip));
    // Second column selects the second module, not the row below it.
    click({top.x + initial_width * .5f + 35.f, top.y + 28.f});
    assert(state.selected == entries[1]); frame(2);
    auto* panel = ImGui::FindWindowByName("##grim_settings_panel");
    assert(panel && panel->Active);
    auto* root = ImGui::FindWindowByName("##ThedoriusGrimUI");
    assert(panel->Pos.x >= grid()->Pos.x + grid()->Size.x);
    assert(std::abs(grid()->Size.x - initial_width) < 1.f);
    assert(panel->Pos.x + panel->Size.x <= io.DisplaySize.x);
    const bool enabled_before = entries[0]->enabled;
    top = grid()->Pos;
    const float card_width = (grid()->Size.x - 24.f - 8.f) * .5f;
    // Former toggle/gear area is now part of the card and must only select it.
    click({top.x + 12.f + card_width - 28.f, top.y + 33.f});
    assert(entries[0]->enabled == enabled_before && state.selected == entries[0]);
    auto* content = settings_content();
    // Enabled is the first row, above Keybind, and never closes the panel.
    click({content->Pos.x + content->Size.x - 26.f, content->Pos.y + 20.f});
    assert(entries[0]->enabled != enabled_before && state.selected == entries[0]);
    assert(entries[0]->get_keybind() == 0);
    // Hovering Enabled must not produce the old full-width row rectangle.
    const auto row_start = ImVec2(content->Pos.x, content->Pos.y + content->WindowPadding.y);
    for (const auto& vertex : content->DrawList->VtxBuffer)
        assert(!(vertex.pos.x > row_start.x + 4.f && vertex.pos.x < row_start.x + content->Size.x - 4.f &&
            vertex.pos.y >= row_start.y - .1f && vertex.pos.y <= row_start.y + 1.f && (vertex.col & IM_COL32_A_MASK)));
    capture(capture_dir, "combat-settings");
    if (!capture_dir.empty())
        capture_imgui(capture_dir + "/settings-detail.ppm", 2.f,
            {panel->Pos.x - 16.f, panel->Pos.y - 16.f}, {panel->Size.x + 32.f, panel->Size.y + 32.f});
    click({panel->Pos.x + 280.f, panel->Pos.y + 26.f});
    assert(!state.selected && !panel->Active);
    state.selected = entries[0]; frame(3);
    entries[0]->begin_keybind_capture(); last_key = VK_ESCAPE;
    key(ImGuiKey_Escape);
    assert(gui::draw && !entries[0]->is_keybind_capture_active() && entries[0]->get_keybind() == 0);
    last_key = 0;
    // Keybind-only modules toggle directly, via search/Enter and full-card
    // clicks, while remaining assignable in the central Keybinds page.
    modules::module plain("full bright", "Visual", "full_bright");
    entries.push_back(&plain);
    open_search_result(&plain); frame(3);
    assert(!has_settings_panel(&plain) && !state.selected && !panel->Active && plain.enabled);
    assert(gui::keybinds_model::assign(plain,'G') && plain.get_keybind() == 'G');
    top = grid()->Pos;
    click({top.x + 30.f,top.y + 28.f});
    assert(!plain.enabled && !state.selected && !panel->Active && !gui::should_unload);
    click({top.x + 30.f,top.y + 28.f});
    assert(plain.enabled && !panel->Active && plain.get_keybind() == 'G');
    clear_search(); state.focus_search = true; frame(3);
    io.AddInputCharactersUTF8("full bright"); frame(3);
    key(ImGuiKey_Enter);
    assert(!plain.enabled && !state.selected && !panel->Active && !state.search[0]);
    information_only information;
    entries.push_back(&information);
    open_search_result(&information); frame(3);
    assert(information.enabled && !has_settings_panel(&information) && !panel->Active);
    entries.pop_back();
    entries.pop_back(); state.current = page::combat; state.selected = entries[0]; frame(3);
    // Long settings remain scrollable without a scrollbar; switching modules
    // gives a fresh child scroll state instead of inheriting the previous one.
    owned[0]->extra_rows = 35; frame(3);
    ImGuiWindow* options = nullptr;
    for (auto* child : panel->DC.ChildWindows) options = child;
    assert(options && options->ScrollMax.y > 0.f && !options->ScrollbarY);
    io.AddMousePosEvent(options->Pos.x + 60, options->Pos.y + 140); frame(2);
    io.AddMouseWheelEvent(0,-5); frame(5);
    assert(options->Scroll.y > 0.f);
    state.selected = entries[2]; frame(3);
    for (auto* child : panel->DC.ChildWindows) if (child->Active) options = child;
    assert(options->Scroll.y == 0.f);
    // Other pages stay full width and never retain an active module panel.
    state.current = page::configs; state.selected = nullptr; frame(3);
    assert(configs_rendered && !panel->Active);
    state.current = page::friends; frame(3); assert(friends_rendered);
    state.current = page::hud; frame(3);
    capture(capture_dir, "hud-controls");
    auto* hud_surface = active_window("##hud_surface");
    assert(hud_surface->ContentSize.y > 220.f); // Three readable HUD rows and controls.
    io.DisplaySize = {900,700}; frame(3);
    hud_surface = active_window("##hud_surface");
    assert(hud_surface->Pos.x + hud_surface->Size.x <= io.DisplaySize.x);
    capture(capture_dir, "hud-controls-narrow");
    io.DisplaySize = {1600,900}; frame(3);
    state.current = page::friends; frame(3);
    // Navigate via the actual Other > Keybinds sidebar button.
    root = ImGui::FindWindowByName("##ThedoriusGrimUI");
    click({root->Pos.x + 70.f,root->Pos.y + 488.f});
    assert(state.current == page::keybinds); frame(3);
    capture(capture_dir,"keybinds-keyboard");
    select_keyboard_key(0x75); // F6
    const bool was_enabled = entries[0]->enabled;
    select_binding_row(0);
    assert(entries[0]->get_keybind() == 0x75 && entries[0]->enabled == was_enabled);
    select_binding_row(1);
    assert(entries[1]->get_keybind() == 0x75 && gui::keybinds_model::counts()[0x75] == 2);
    select_keyboard_key(platform::keys::num_enter);
    select_binding_row(0);
    assert(entries[0]->get_keybind() == platform::keys::num_enter && entries[1]->get_keybind() == 0x75);
    io.AddMousePosEvent(-100,-100); frame(2);
    capture(capture_dir,"keybinds-assignment");
    assert(configs::keybind_profile::initialize(profile_path));
    assert(entries[0]->get_keybind() == platform::keys::num_enter);
    // The visually unified category control must open from either its label
    // or arrow, and Escape must dismiss the popup without closing the menu.
    for (float offset : {100.f,12.f}) {
        const auto* list = active_window("##module_selection");
        click({list->Pos.x + list->Size.x - offset,list->Pos.y - 23.f});
        assert(ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel));
        key(ImGuiKey_Escape);
        assert(gui::draw && !ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel));
    }
    // Hit-test every physical position using the shipping UI, not just the
    // layout model. All assignable keys must reach the same real module field.
    for (const auto& keyboard_key : platform::keys::keyboard) {
        select_keyboard_key(keyboard_key.code);
        const int previous_binding = entries[0]->get_keybind();
        select_binding_row(0);
        if (platform::keys::assignable(keyboard_key.code))
            assert(entries[0]->get_keybind() == keyboard_key.code);
        else assert(entries[0]->get_keybind() == previous_binding);
        assert(entries[0]->enabled == was_enabled);
    }
    select_keyboard_key(platform::keys::num_enter); select_binding_row(0);
    assert(entries[0]->get_keybind() == platform::keys::num_enter);
    auto* module_list = active_window("##module_selection");
    assert(module_list->ScrollMax.y > 0.f && !module_list->ScrollbarY);
    io.AddMousePosEvent(module_list->Pos.x + 50.f,module_list->Pos.y + 70.f); frame(2);
    io.AddMouseWheelEvent(0,-3); frame(4); assert(module_list->Scroll.y > 0.f);
    io.AddMouseWheelEvent(0,100); frame(4); assert(module_list->Scroll.y == 0.f);
    select_keyboard_key(0x23); // Reserved End: assignment blocked, clear remains usable.
    select_binding_row(0); assert(entries[0]->get_keybind() == platform::keys::num_enter);
    select_binding_row(0,true); assert(entries[0]->get_keybind() == 0);
    entries[0]->set_keybind(0x23); frame(2); select_binding_row(0);
    assert(entries[0]->get_keybind() == 0); // Remove an imported reserved binding.
    select_keyboard_key('H');
    auto* selection_list = active_window("##module_selection");
    // Clicking the trailing magnifier focuses the same real search field.
    click({selection_list->Pos.x + selection_list->Size.x - 190.f,selection_list->Pos.y - 23.f});
    assert(ImGui::GetActiveID() == active_window("##keybinds_surface")->GetID("##module_search"));
    io.AddInputCharactersUTF8("shield"); frame(3);
    assert(active_window("##module_selection")->ContentSize.y < 100.f);
    select_binding_row(0); assert(entries.back()->get_keybind() == 'H');
    assert(state.search[0] == 0); // Not the unrelated sidebar search field.
    // All keyboard rectangles stay inside the surface after viewport changes.
    for (ImVec2 viewport : {ImVec2(1024,720),ImVec2(900,700),ImVec2(1600,900)}) {
        io.DisplaySize = viewport; frame(4);
        const auto* surface = active_window("##keybinds_surface");
        assert(surface->ScrollMax.x == 0.f && surface->Pos.x + surface->Size.x <= viewport.x);
    }
    entries[0]->set_keybind(0); frame(2);
    state.current = page::combat; state.selected = entries[0]; frame(3);
    // An offset or a smaller viewport must still leave the right panel visible.
    state.position = {10000.f,10000.f}; io.DisplaySize = {1280,720}; frame(4);
    assert(panel->Pos.x + panel->Size.x <= 1280.f && panel->Pos.y + panel->Size.y <= 720.f);
    capture(capture_dir, "combat-1280");
    io.DisplaySize = {1600,900};
    const auto recentered = gui::dock_layout::main_panel({0,0,1600,900});
    state.position = {recentered.x, recentered.y}; frame(3);
    ::preferences.reduce_motion = false;
    state.selected = nullptr; frame(60);
    assert(!panel->Active && !state.settings_module);
    ::preferences.reduce_motion = true;
    auto* saved_bold = gui::ui_bold_font;
    gui::ui_bold_font = nullptr; frame(); gui::ui_bold_font = saved_bold;
    clear_search(); state.focus_search = true; frame(3);
    io.AddInputCharactersUTF8("shield"); frame(3);
    assert(search_results().size() == 1);
    key(ImGuiKey_Enter); assert(state.selected == entries.back() && !state.search[0]);
    frame(3);
    key(ImGuiKey_Escape); assert(!gui::draw && gui::suppress_escape_until_release.load());
    // Panic remains an immediate action rather than a persistent enabled flag.
    modules::module panic("panic", "Client", "panic");
    entries.push_back(&panic); state.current = page::client; state.selected = nullptr;
    gui::draw = true; frame(3);
    top = grid()->Pos; click({top.x + 30.f, top.y + 28.f});
    assert(gui::should_unload && !gui::draw && !panic.enabled && !has_settings_panel(&panic));
    entries.pop_back();
    gui::grim::reset_runtime_state(); gui::reset_widget_runtime_state();
    assert(!state.selected && !state.settings_module && state.options_content_height.empty() && state.card_layouts.empty());
    {
        // Fixed shortcuts share the same bounded badge; long values stack, not shrink.
        auto controls_frame = [&] {
            ImGui::NewFrame(); ImGui::SetNextWindowPos({20,20}); ImGui::SetNextWindowSize({296,240});
            ImGui::Begin("Control regression fixture",nullptr,ImGuiWindowFlags_NoSavedSettings);
            fixed.key_bind_selector(132.f);
            assert(ImGui::GetItemRectSize().x==132.f);
            const char* normal[]={"Normal"}; int choice=0;
            gui::option_combo("Mode",&choice,normal,1);
            assert(ImGui::GetItemRectSize().y==36.f);
            const char* long_value[]={"Work without inventory switching"};
            gui::option_combo("Mode",&choice,long_value,1);
            assert(ImGui::GetItemRectSize().y==60.f);
            ImGui::End(); ImGui::Render();
        };
        controls_frame(); controls_frame();
        auto border_colors = [&](bool reduce_motion) {
            ::preferences.reduce_motion=reduce_motion;
            ImGui::NewFrame(); ImGui::SetNextWindowPos({20,20}); ImGui::SetNextWindowSize({320,180});
            ImGui::Begin("Flowing border fixture",nullptr,ImGuiWindowFlags_NoSavedSettings);
            auto* draw=ImGui::GetWindowDrawList(); const int first=draw->VtxBuffer.Size;
            flowing_card_border(draw,{40,60},{300,140},.85f);
            std::vector<ImU32> colors;
            for (int index=first; index<draw->VtxBuffer.Size; ++index) colors.push_back(draw->VtxBuffer[index].col);
            ImGui::Dummy({260,80}); ImGui::End(); ImGui::Render(); return colors;
        };
        const auto first=border_colors(false), next=border_colors(false);
        assert(!first.empty() && first!=next);
        const auto still=border_colors(true); assert(still==border_colors(true));
    }
    ImGui::DestroyContext();
    std::cout << "PASS: real GrimUI cards/settings, Other > Keybinds navigation, keyboard assignment/move/remove/shared keys, dynamic categories/search, plain modules, reserved keys, profile restart/corruption/I/O failure, public config roundtrip, viewport bounds, Escape and reset\n";
}
