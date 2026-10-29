#pragma once
#include <imgui/imgui.h>

namespace gui::hud_editor
{
    void render_page();
    void render(const ImGuiViewport* viewport);
    void restore_saved_layout();
    void close();
}
