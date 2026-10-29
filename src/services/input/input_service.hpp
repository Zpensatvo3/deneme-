#pragma once

#include <Windows.h>
#include "click_tracking.hpp"
#include "jump_key_tracking.hpp"
#include <functional>

namespace services::input
{
    enum class mouse_button { left, right };

    bool mouse_button_state(mouse_button button, bool pressed);
    // Client-thread vanilla Mouse callback. Requires a real held button and
    // does not alter Windows input state or manufacture hardware transitions.
    bool game_mouse_click(mouse_button button);
    bool mouse_click(mouse_button button,
        HWND message_window = nullptr,
        bool preserve_physical_press = false);
    bool key_scan_state(UINT virtual_key, bool pressed);
    bool key_pulse(UINT virtual_key);
    bool move_relative(long x, long y);
    bool is_synthetic_tag(std::uintptr_t tag);
    void observe_physical(mouse_button button, bool pressed);
    bool physical_button_down(mouse_button button);
    std::uint64_t manual_attack_generation();
    void reset_physical_buttons();
    void observe_physical_key(unsigned virtual_key, unsigned scan_code, bool pressed);
    bool physical_key_down(unsigned virtual_key);
    bool physical_scan_down(unsigned scan_code);
    void reset_physical_keys();
    using click_validation = std::function<bool()>;
    // The validator runs only on the submitting client thread, outside the
    // ledger lock, immediately before forwarding each attack-producing edge.
    click_ticket submit_trigger_click(HWND expected_window, bool preserve_physical_press,
        click_validation validate = click_validation{});
    click_receipt trigger_click_receipt(std::uint32_t ticket);
    void cancel_trigger_click(std::uint32_t ticket = 0);
    void cancel_automatic_inputs();
    bool observe_tracked_event(std::uintptr_t tag, bool pressed, event_path path);
    bool accept_restore_event(std::uintptr_t tag);
    std::uint32_t press_jump_space(HWND expected_window, click_validation validate);
    bool release_jump_space(std::uint32_t ticket);
    click_receipt jump_space_receipt(std::uint32_t ticket);
    bool observe_jump_space_event(std::uintptr_t tag, bool pressed);
}
