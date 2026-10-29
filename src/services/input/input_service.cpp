#include "input_service.hpp"

#include "../../modules/core/module.hpp"
#include "../../services/game/client_context.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../gui/gui.hpp"
#include <atomic>
#include <mutex>
#include <array>

namespace
{
    std::atomic_bool physical_left{false}, physical_right{false};
    std::atomic<std::uint64_t> manual_generation{0};
    std::mutex ledger_mutex;
    services::input::click_ledger ledger;
    HWND ticket_window = nullptr;
    DWORD ticket_thread = 0;
    services::input::click_validation ticket_validation;
    std::array<std::atomic_bool, 256> physical_keys{};
    std::array<std::atomic_bool, 512> physical_scans{};
    std::mutex jump_mutex;
    services::input::jump_key_ledger jump_ledger;
    HWND jump_window = nullptr;
    DWORD jump_thread = 0;
    services::input::click_validation jump_validation;
    DWORD mouse_flag(services::input::mouse_button button, bool pressed)
    {
        if (button == services::input::mouse_button::left)
            return pressed ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
        return pressed ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
    }
    void restore_trigger_hold(std::uint32_t id, HWND expected_window)
    {
        if (!id || !expected_window || GetForegroundWindow() != expected_window) return;
        const bool held = services::input::physical_button_down(services::input::mouse_button::left);
        INPUT input{};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = mouse_flag(services::input::mouse_button::left, held);
        input.mi.dwExtraInfo = services::input::restore_ticket_tag(id, held);
        // WndProc restores hold state only; this is not another vanilla click.
        SendInput(1, &input, sizeof(INPUT));
    }
}

bool services::input::is_synthetic_tag(std::uintptr_t tag)
{
    return static_cast<std::uint32_t>(tag) == static_cast<std::uint32_t>(modules::synthetic_mouse_tag) || tracked_tag(tag) || restore_tag(tag) || jump_key_tag(tag);
}
void services::input::observe_physical(mouse_button button, bool pressed)
{
    auto& value = button == mouse_button::left ? physical_left : physical_right;
    const bool previous = value.exchange(pressed);
    physical_keys[button == mouse_button::left ? VK_LBUTTON : VK_RBUTTON].store(pressed);
    if (button == mouse_button::left && pressed && !previous) ++manual_generation;
}
bool services::input::physical_button_down(mouse_button button)
{
    return (button == mouse_button::left ? physical_left : physical_right).load();
}
std::uint64_t services::input::manual_attack_generation() { return manual_generation.load(); }
void services::input::observe_physical_key(unsigned key, unsigned scan, bool pressed)
{
    if (key < physical_keys.size()) physical_keys[key].store(pressed);
    if (scan && scan < physical_scans.size()) physical_scans[scan].store(pressed);
    unsigned side = key;
    if (key == VK_SHIFT) side = MapVirtualKeyW(scan, MAPVK_VSC_TO_VK_EX);
    else if (key == VK_CONTROL) side = scan & 0x100u ? VK_RCONTROL : VK_LCONTROL;
    else if (key == VK_MENU) side = scan & 0x100u ? VK_RMENU : VK_LMENU;
    if (side < physical_keys.size()) physical_keys[side].store(pressed);
}
bool services::input::physical_key_down(unsigned key)
{
    return key < physical_keys.size() && physical_keys[key].load();
}
bool services::input::physical_scan_down(unsigned scan)
{
    return scan < physical_scans.size() && physical_scans[scan].load();
}
void services::input::reset_physical_keys()
{
    for (auto& key : physical_keys) key.store(false);
    for (auto& scan : physical_scans) scan.store(false);
}

std::uint32_t services::input::press_jump_space(HWND expected_window, click_validation validate)
{
    if (!expected_window || GetForegroundWindow() != expected_window || physical_key_down(VK_SPACE)) return 0;
    std::uint32_t id;
    {
        std::lock_guard lock(jump_mutex); id = jump_ledger.reserve();
        if (id) { jump_window = expected_window; jump_thread = GetCurrentThreadId(); jump_validation = std::move(validate); }
    }
    if (!id) return 0;
    INPUT event{}; event.type = INPUT_KEYBOARD; event.ki.wScan = static_cast<WORD>(MapVirtualKeyW(VK_SPACE, MAPVK_VK_TO_VSC));
    event.ki.dwFlags = KEYEVENTF_SCANCODE; event.ki.dwExtraInfo = jump_key_event_tag(id, true);
    if (SendInput(1, &event, sizeof(INPUT)) == 1) return id;
    // With a one-event request, rejection inserted no DOWN. No key-up cleanup
    // is necessary; do not leave an undelivered lease blocking future requests.
    {
        std::lock_guard lock(jump_mutex);
        if (id == jump_ledger.active) {
            jump_ledger.cancel(id); jump_ledger.up_queued = true; jump_validation = {};
        }
    }
    return 0;
}
bool services::input::release_jump_space(std::uint32_t id)
{
    {
        std::lock_guard lock(jump_mutex);
        if (!id || id != jump_ledger.active) return true;
        if (jump_ledger.cancelled && jump_ledger.up_queued) return true;
        jump_ledger.cancel(id); jump_validation = {};
    }
    // Never release the user's real Space hold. A queued UP is also filtered
    // again at delivery if a physical press arrives after this check.
    if (physical_key_down(VK_SPACE)) {
        std::lock_guard lock(jump_mutex); if (id == jump_ledger.active) jump_ledger.up_queued = true;
        return true;
    }
    INPUT event{}; event.type = INPUT_KEYBOARD; event.ki.wScan = static_cast<WORD>(MapVirtualKeyW(VK_SPACE, MAPVK_VK_TO_VSC));
    event.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP; event.ki.dwExtraInfo = jump_key_event_tag(id, false);
    const bool sent = SendInput(1, &event, sizeof(INPUT)) == 1;
    if (sent) { std::lock_guard lock(jump_mutex); if (id == jump_ledger.active) jump_ledger.up_queued = true; }
    return sent;
}
services::input::click_receipt services::input::jump_space_receipt(std::uint32_t id)
{
    std::lock_guard lock(jump_mutex);
    if (!id || id != jump_ledger.active) return {false, true};
    return {jump_ledger.delivered, jump_ledger.cancelled};
}
bool services::input::observe_jump_space_event(std::uintptr_t tag, bool pressed)
{
    click_validation validate;
    std::uint32_t id; HWND expected_window; DWORD expected_thread;
    {
        std::lock_guard lock(jump_mutex);
        if (!pressed || jump_ledger.cancelled)
            return jump_ledger.observe(tag, pressed, physical_key_down(VK_SPACE));
        id = jump_ledger.active; validate = jump_validation;
        expected_window = jump_window; expected_thread = jump_thread;
    }
    bool valid = expected_window && GetForegroundWindow() == expected_window && GetCurrentThreadId() == expected_thread;
    if (valid && validate) { try { valid = validate(); } catch (...) { valid = false; } }
    bool allowed, rejected;
    {
        std::lock_guard lock(jump_mutex);
        if (id != jump_ledger.active) return false;
        allowed = jump_ledger.observe(tag, pressed, physical_key_down(VK_SPACE), valid);
        rejected = jump_ledger.cancelled;
        if (rejected) jump_validation = {};
    }
    if (rejected) release_jump_space(id);
    return allowed;
}
void services::input::reset_physical_buttons()
{
    physical_left.store(false); physical_right.store(false); ++manual_generation;
}
services::input::click_ticket services::input::submit_trigger_click(HWND expected_window, bool preserve, click_validation validate)
{
    if (!expected_window || !IsWindow(expected_window) || GetForegroundWindow() != expected_window) return {};
    std::uint32_t id;
    {
        std::lock_guard lock(ledger_mutex); id = ledger.reserve(preserve);
        if (id) {
            ticket_window = expected_window; ticket_thread = GetCurrentThreadId();
            ticket_validation = std::move(validate);
        }
    }
    if (!id) return {};
    INPUT inputs[2]{};
    for (int index = 0; index < 2; ++index) {
        const bool pressed = index == 0 ? !preserve : preserve;
        inputs[index].type = INPUT_MOUSE;
        inputs[index].mi.dwFlags = mouse_flag(mouse_button::left, pressed);
        inputs[index].mi.dwExtraInfo = ticket_tag(id, pressed);
    }
    const UINT sent = SendInput(2, inputs, sizeof(INPUT));
    const auto result = classify_submission(sent);
    if (sent != 2) {
        cancel_trigger_click(id);
        // SendInput may have enqueued the first edge even if WndProc has not
        // observed it yet. Restore hold without generating another attack.
        if (sent == 1 && GetForegroundWindow() == expected_window)
            restore_trigger_hold(id, expected_window);
    }
    return { id, result, input_source::triggerbot };
}
services::input::click_receipt services::input::trigger_click_receipt(std::uint32_t id)
{
    std::lock_guard lock(ledger_mutex); return ledger.receipt(id);
}
void services::input::cancel_trigger_click(std::uint32_t id)
{
    bool repair = false;
    HWND expected_window = nullptr;
    {
        std::lock_guard lock(ledger_mutex);
        if (!id) id = ledger.active;
        if (id && id == ledger.active && !ledger.cancelled) {
            repair = ledger.needs_repair();
            ledger.cancel(id);
            ticket_validation = {};
            expected_window = ticket_window;
        }
    }
    if (repair && expected_window && GetForegroundWindow() == expected_window)
        restore_trigger_hold(id, expected_window);
}
void services::input::cancel_automatic_inputs()
{
    cancel_trigger_click();
    std::uint32_t jump;
    { std::lock_guard lock(jump_mutex); jump=jump_ledger.active; }
    if(jump)release_jump_space(jump);
}
bool services::input::observe_tracked_event(std::uintptr_t tag, bool pressed, event_path path)
{
    click_validation validate;
    std::uint32_t id = 0;
    HWND expected_window = nullptr;
    DWORD expected_thread = 0;
    {
        std::lock_guard lock(ledger_mutex);
        if (!ledger.needs_validation(tag, pressed, path))
            return ledger.observe(tag, pressed, path, physical_left.load());
        id = ledger.active; expected_window = ticket_window;
        expected_thread = ticket_thread; validate = ticket_validation;
    }
    // Never hold the ledger mutex across JNI or a user-supplied validator.
    // A concurrent disable can cancel the ticket while validation is in flight.
    bool valid = expected_window && GetForegroundWindow() == expected_window;
    if (validate) {
        valid = valid && GetCurrentThreadId() == expected_thread;
        if (valid) { try { valid = validate(); } catch (...) { valid = false; } }
    }
    bool repair = false, allowed = false;
    {
        std::lock_guard lock(ledger_mutex);
        if (ledger.active != id) return false;
        repair = !valid && !ledger.cancelled && ledger.needs_repair();
        allowed = ledger.observe(tag, pressed, path, physical_left.load(), valid);
        if (ledger.cancelled) ticket_validation = {};
    }
    if (repair) restore_trigger_hold(id, expected_window);
    return allowed;
}
bool services::input::accept_restore_event(std::uintptr_t tag)
{
    std::lock_guard lock(ledger_mutex); return ledger.accepts_restore(tag);
}

bool services::input::mouse_button_state(mouse_button button, bool pressed)
{
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = mouse_flag(button, pressed);
    input.mi.dwExtraInfo = static_cast<ULONG_PTR>(modules::synthetic_mouse_tag);
    return SendInput(1, &input, sizeof(input)) == 1;
}

bool services::input::game_mouse_click(mouse_button button)
{
    const HWND expected_window = gui::get_window();
    if (!expected_window || GetForegroundWindow() != expected_window || gui::draw || gui::loader_visible ||
        gui::should_unload || !physical_button_down(button) || !platform::jni_context::bind_current_thread()) return false;
    jni::frame frame{48};
    services::game::client_context context;
    if (!services::game::capture_client_context(context, true, false) || context.screen) return false;
    auto mouse = context.instance.mouse.get();
    auto window = context.instance.getWindow.call();
    auto options = context.instance.options.get();
    if (platform::jni_context::clear_exception() || !mouse || !window || !options ||
        !mouse.onMouseButton.is_valid() || !window.getHandle.is_valid()) return false;
    auto binding = button == mouse_button::left ? options.attackKey.get() : options.useKey.get();
    if (platform::jni_context::clear_exception() || !binding || !binding.setPressed.is_valid()) return false;
    const auto handle = window.getHandle.call();
    if (platform::jni_context::clear_exception() || !handle) return false;
    const int index = button == mouse_button::left ? 0 : 1;
    // Same route as GLFW's callback, without queued SendInput edges racing a
    // physical release. UP/DOWN queues exactly one vanilla key press.
    mouse.onMouseButton.call(handle, index, 0, 0);
    bool failed = platform::jni_context::clear_exception();
    if (!failed) {
        mouse.onMouseButton.call(handle, index, 1, 0);
        failed = platform::jni_context::clear_exception();
    }
    if (failed) {
        const jboolean held = physical_button_down(button) ? JNI_TRUE : JNI_FALSE;
        binding.setPressed.call(held);
        if (button == mouse_button::left) mouse.leftButtonClicked.set(held);
        else mouse.rightButtonClicked.set(held);
        platform::jni_context::clear_exception();
    }
    return !failed;
}

bool services::input::mouse_click(mouse_button button,
    HWND message_window, bool preserve_physical_press)
{
    INPUT inputs[2]{};
    for (INPUT& input : inputs)
    {
        input.type = INPUT_MOUSE;
        input.mi.dwExtraInfo = static_cast<ULONG_PTR>(modules::synthetic_mouse_tag);
    }
    // When preserve_physical_press is true, cycle UP then DOWN so the user's physical hold
    // is preserved and not cancelled. When false, click DOWN then UP for one-shot clicks.
    inputs[0].mi.dwFlags = mouse_flag(button, !preserve_physical_press);
    inputs[1].mi.dwFlags = mouse_flag(button, preserve_physical_press);
    return SendInput(2, inputs, sizeof(INPUT)) == 2;
}

bool services::input::key_scan_state(UINT virtual_key, bool pressed)
{
    const UINT scan_code = MapVirtualKeyW(virtual_key, MAPVK_VK_TO_VSC);
    if (!scan_code)
        return false;

    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wScan = static_cast<WORD>(scan_code);
    input.ki.dwFlags = KEYEVENTF_SCANCODE |
        (pressed ? 0u : static_cast<DWORD>(KEYEVENTF_KEYUP));
    input.ki.dwExtraInfo = static_cast<ULONG_PTR>(modules::synthetic_mouse_tag);
    return SendInput(1, &input, sizeof(input)) == 1;
}

bool services::input::key_pulse(UINT virtual_key)
{
    INPUT inputs[2]{};
    for (INPUT& input : inputs)
    {
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = static_cast<WORD>(virtual_key);
        input.ki.wScan = static_cast<WORD>(MapVirtualKeyW(virtual_key, MAPVK_VK_TO_VSC));
        input.ki.dwExtraInfo = static_cast<ULONG_PTR>(modules::synthetic_mouse_tag);
    }
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    return SendInput(2, inputs, sizeof(INPUT)) == 2;
}

bool services::input::move_relative(long x, long y)
{
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dx = x;
    input.mi.dy = y;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    input.mi.dwExtraInfo = static_cast<ULONG_PTR>(modules::synthetic_mouse_tag);
    return SendInput(1, &input, sizeof(input)) == 1;
}
