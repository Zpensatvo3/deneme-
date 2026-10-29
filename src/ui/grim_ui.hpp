#pragma once

#include <imgui/imgui.h>

namespace gui::grim
{
    struct page_callbacks
    {
        void (*friends)() = nullptr;
        void (*configs)() = nullptr;
    };

    void render(const ImGuiViewport* viewport, const page_callbacks& callbacks);
    void reset_runtime_state();
}
