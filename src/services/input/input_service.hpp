#pragma once

#include <Windows.h>
#include <cstdint>

namespace services::input
{
    enum class mouse_button { left, right };

    // Pearl Macro still uses the native fallback for some targets.
    bool mouse_button_state(mouse_button button, bool pressed);
    bool is_synthetic_tag(std::uintptr_t tag);
    void observe_physical(mouse_button button, bool pressed);
    bool physical_button_down(mouse_button button);
    std::uint64_t manual_attack_generation();
    void reset_physical_buttons();
    void observe_physical_key(unsigned virtual_key, unsigned scan_code, bool pressed);
    bool physical_key_down(unsigned virtual_key);
    bool physical_scan_down(unsigned scan_code);
    void reset_physical_keys();
}
