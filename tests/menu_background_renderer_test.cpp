#include "../src/ui/menu_background.hpp"
#include "../src/gui/ui_state.hpp"
#include "../src/gui/gui.hpp"
#include "../src/ui/themes_page.hpp"
#include <cassert>
#include <cfloat>
#include <cstdint>

namespace
{
    gui::ui_state::preferences preferences;
    int dirty_count = 0;
    auto image_id(int index) { return reinterpret_cast<ImTextureID>(std::uintptr_t(10 + index)); }
}
namespace gui::ui_state
{
    preferences& get_preferences() { return ::preferences; }
    void mark_preferences_dirty() { ++dirty_count; }
}
namespace gui
{
    ImColor get_dynamic_color(int) { return ImColor(155, 69, 255); }
}
namespace gui::background
{
    texture_view texture(style selected)
    {
        if (selected == style::classic || selected == style::custom) return {};
        return {image_id(static_cast<int>(selected)), {2000, 1000}};
    }
    import_state status() { return import_state::idle; }
    void begin_import() { assert(false && "file picker must not open during unrelated card clicks"); }
}

int main()
{
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr; io.LogFilename = nullptr;
    io.DisplaySize = {1000, 700}; io.DeltaTime = 1.f / 60;
    unsigned char* pixels; int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.Fonts->SetTexID(image_id(0));

    ImGui::NewFrame();
    auto* draw = ImGui::GetBackgroundDrawList();
    gui::background::options value;
    assert(!gui::background::render(draw, {20, 20}, {960, 620}, value));
    assert(draw->VtxBuffer.empty()); // Classic appearance adds absolutely nothing.
    value.selected = gui::background::style::custom;
    assert(!gui::background::render(draw, {20, 20}, {960, 620}, value)); // Missing file.
    value.selected = gui::background::style::nebula;
    value.opacity = 0;
    assert(!gui::background::render(draw, {20, 20}, {960, 620}, value));
    value.opacity = .85f;
    assert(gui::background::render(draw, {20, 20}, {960, 620}, value));
    for (const auto& vertex : draw->VtxBuffer)
    {
        assert(vertex.pos.x >= 19 && vertex.pos.x <= 981);
        assert(vertex.pos.y >= 19 && vertex.pos.y <= 641);
    }
    ImGui::Render();

    auto settings_frame = [&]
    {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({20, 20});
        ImGui::SetNextWindowSize({740, 600});
        ImGui::Begin("Background settings", nullptr, ImGuiWindowFlags_NoSavedSettings);
        gui::background::render_settings(IM_COL32(155, 69, 255, 255));
        ImGui::End();
        ImGui::Render();
    };
    settings_frame(); settings_frame();
    // Find the real thumbnail's screen bounds from its draw command, not hardcoded layout coordinates.
    ImVec2 minimum(FLT_MAX, FLT_MAX), maximum(-FLT_MAX, -FLT_MAX);
    const auto* data = ImGui::GetDrawData();
    for (int list_index = 0; list_index < data->CmdListsCount; ++list_index)
    {
        const auto* list = data->CmdLists[list_index];
        for (const auto& command : list->CmdBuffer)
            if (command.GetTexID() == image_id(1))
                for (unsigned index = command.IdxOffset; index < command.IdxOffset + command.ElemCount; ++index)
                {
                    const auto position = list->VtxBuffer[list->IdxBuffer[index] + command.VtxOffset].pos;
                    minimum = {std::min(minimum.x, position.x), std::min(minimum.y, position.y)};
                    maximum = {std::max(maximum.x, position.x), std::max(maximum.y, position.y)};
                }
    }
    assert(minimum.x != FLT_MAX);
    io.AddMousePosEvent((minimum.x + maximum.x) / 2, (minimum.y + maximum.y) / 2);
    io.AddMouseButtonEvent(0, true); settings_frame();
    io.AddMouseButtonEvent(0, false); settings_frame();
    assert(preferences.menu_background.selected == gui::background::style::citadel);
    assert(dirty_count == 1);
    settings_frame();
    assert(ImGui::GetDrawData()->TotalVtxCount > 0);
    // Exercise the shipping Themes page too, including its short ui_state::
    // namespace lookups. Pure background fixtures cannot catch those link errors.
    io.AddMousePosEvent(-100, -100);
    const float rounding = ImGui::GetStyle().FrameRounding;
    const auto selected = preferences.menu_background.selected;
    for (int pass = 0; pass < 2; ++pass) {
        ImGui::NewFrame();
        ImGui::SetNextWindowSize({740, 600});
        ImGui::Begin("Themes", nullptr, ImGuiWindowFlags_NoSavedSettings);
        gui::themes_page::render();
        ImGui::End();
        ImGui::Render();
    }
    assert(ImGui::GetDrawData()->TotalVtxCount > 0);
    assert(ImGui::GetStyle().FrameRounding == rounding);
    assert(preferences.menu_background.selected == selected && dirty_count == 1);
    ImGui::DestroyContext();
}
