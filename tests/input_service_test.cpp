#include <Windows.h>
#include <cstdint>
#include <cassert>
#include <cstdio>

#ifndef _WIN32
using UINT = unsigned;
using ULONG_PTR = std::uintptr_t;
struct INPUT { DWORD type; struct { DWORD dwFlags; ULONG_PTR dwExtraInfo; } mi; };
using LPINPUT = INPUT*;
constexpr DWORD INPUT_MOUSE = 0, MOUSEEVENTF_LEFTDOWN = 2, MOUSEEVENTF_LEFTUP = 4;
constexpr DWORD MOUSEEVENTF_RIGHTDOWN = 8, MOUSEEVENTF_RIGHTUP = 16;
constexpr int VK_SHIFT = 0x10, VK_CONTROL = 0x11, VK_LCONTROL = 0xA2, VK_RCONTROL = 0xA3;
constexpr unsigned MAPVK_VSC_TO_VK_EX = 3;
#endif

namespace {
    INPUT last_input{};
    UINT delivered = 1;
    unsigned submissions = 0;
    UINT fixture_send_input(UINT count, LPINPUT input, int) {
        assert(count == 1); last_input = *input; ++submissions; return delivered;
    }
    UINT fixture_map_key(UINT scan, UINT mode) {
        assert(mode == MAPVK_VSC_TO_VK_EX); return scan == 0x36 ? VK_RSHIFT : 0xA0;
    }
}
// Compile the shipping input service against inert OS calls. Never inject input.
#define SendInput fixture_send_input
#define MapVirtualKeyW fixture_map_key
#include "../src/services/input/input_service.cpp"
#undef MapVirtualKeyW
#undef SendInput

int main()
{
    using namespace services::input;
    reset_physical_buttons(); reset_physical_keys();
    const auto initial = manual_attack_generation();
    observe_physical(mouse_button::left, true);
    observe_physical(mouse_button::left, true);
    assert(physical_button_down(mouse_button::left) && physical_key_down(VK_LBUTTON));
    assert(manual_attack_generation() == initial + 1);
    observe_physical(mouse_button::left, false);
    observe_physical(mouse_button::left, true);
    assert(manual_attack_generation() == initial + 2);
    observe_physical(mouse_button::right, true);
    assert(physical_key_down(VK_RBUTTON) && manual_attack_generation() == initial + 2);
    observe_physical_key(VK_SHIFT, 0x36, true);
    assert(physical_key_down(VK_RSHIFT) && physical_scan_down(0x36));
    observe_physical_key(VK_CONTROL, 0x11D, true);
    assert(physical_key_down(VK_RCONTROL) && !physical_key_down(VK_LCONTROL));
    observe_physical_key(VK_MENU, 0x138, true);
    assert(physical_key_down(VK_RMENU));
    // End and NumPad 1 share a VK with NumLock off, but not a scan code.
    observe_physical_key(0x23, 0x4F, true);
    assert(physical_scan_down(0x4F) && !physical_scan_down(0x14F));
    observe_physical_key(0x23, 0x14F, true);
    assert(physical_scan_down(0x14F));
    observe_physical_key(9999, 9999, true);
    assert(!physical_key_down(9999) && !physical_scan_down(9999));
    reset_physical_buttons(); reset_physical_keys();
    assert(!physical_button_down(mouse_button::left) && !physical_button_down(mouse_button::right));
    assert(!physical_key_down(VK_RSHIFT) && !physical_scan_down(0x14F));
    const auto generation = manual_attack_generation();
    assert(mouse_button_state(mouse_button::right, true));
    assert(submissions == 1 && last_input.mi.dwFlags == MOUSEEVENTF_RIGHTDOWN);
    assert(is_synthetic_tag(last_input.mi.dwExtraInfo) && !is_synthetic_tag(0));
    assert(!physical_button_down(mouse_button::right) && manual_attack_generation() == generation);
    delivered = 0;
    assert(!mouse_button_state(mouse_button::right, false));
    assert(last_input.mi.dwFlags == MOUSEEVENTF_RIGHTUP);
    std::puts("PASS physical edges, manual generation, modifier/extended scans, resets and Pearl fallback input");
}
