#include "input_service.hpp"
#include "../../modules/core/module.hpp"
#include <array>
#include <atomic>

namespace
{
    std::atomic_bool physical_left{false}, physical_right{false};
    std::atomic<std::uint64_t> manual_generation{0};
    std::array<std::atomic_bool, 256> physical_keys{};
    std::array<std::atomic_bool, 512> physical_scans{};
    DWORD mouse_flag(services::input::mouse_button button, bool pressed)
    {
        if (button == services::input::mouse_button::left)
            return pressed ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
        return pressed ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
    }
}

bool services::input::is_synthetic_tag(std::uintptr_t tag)
{
    return static_cast<std::uint32_t>(tag) == static_cast<std::uint32_t>(modules::synthetic_mouse_tag);
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

void services::input::reset_physical_keys()
{
    for (auto& key : physical_keys) key.store(false);
    for (auto& scan : physical_scans) scan.store(false);
}

bool services::input::physical_scan_down(unsigned scan)
{
    return scan < physical_scans.size() && physical_scans[scan].load();
}

void services::input::reset_physical_buttons()
{
    physical_left.store(false); physical_right.store(false); ++manual_generation;
}

bool services::input::mouse_button_state(mouse_button button, bool pressed)
{
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = mouse_flag(button, pressed);
    input.mi.dwExtraInfo = static_cast<ULONG_PTR>(modules::synthetic_mouse_tag);
    return SendInput(1, &input, sizeof(input)) == 1;
}
