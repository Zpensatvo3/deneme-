#include "../src/services/game/menu_input_policy.hpp"
#include <cassert>
#include <iostream>

int main()
{
    using namespace services::game::menu_input;
    focused_shortcut shortcut;
    assert(!shortcut.pressed(true, true)); // first focus with a key already held
    assert(!shortcut.pressed(true, false));
    assert(shortcut.pressed(true, true));
    assert(!shortcut.pressed(true, true)); // no autorepeat
    assert(!shortcut.pressed(false, false));
    assert(!shortcut.pressed(false, true)); // presses in another app are ignored
    assert(!shortcut.pressed(true, true)); // held through Alt+Tab is not a new press
    assert(!shortcut.pressed(true, false));
    assert(shortcut.pressed(true, true));
    focused_pointer pointer;
    assert(!pointer.accepts(false, false));
    assert(!pointer.accepts(true, true)); // taskbar click held when returning
    assert(pointer.accepts(true, false));
    assert(pointer.accepts(true, true));
    assert(!pointer.accepts(false, true)); // drag loses focus
    assert(!pointer.accepts(true, true));
    assert(pointer.accepts(true, false));
    assert(!accepts_panel_pointer(true, false));
    assert(!accepts_panel_pointer(false, true));
    assert(accepts_panel_pointer(true, true));
    for (unsigned key : {0x09u, 0x1bu, 0x73u}) {
        assert(system_shortcut(key, true));
        assert(!system_shortcut(key, false));
    }
    assert(system_shortcut(0x5b, false) && system_shortcut(0x5c, false));
    assert(!system_shortcut('W', true));
    using services::game::menu_input::cursor_lease;
    cursor_lease lease;
    assert(!lease.finish(true, true, true));
    lease.begin(true);
    assert(lease.finish(true, true, true));
    assert(!lease.finish(true, true, true)); // no repeated capture on later frames
    for (int reason = 0; reason < 3; ++reason) {
        lease.begin(true);
        assert(!lease.finish(reason != 0, reason != 1, reason != 2));
        assert(!lease.finish(true, true, true)); // no late capture after alt-tab
    }
    lease.begin(false); // inventory/menu cursor was already free
    assert(!lease.finish(true, true, true));
    lease.begin(true);
    lease.invalidate(); // world/player/window changed while panel was open
    assert(!lease.finish(true, true, true));
    lease.begin(true); // fresh open/close still works after invalidation
    assert(lease.finish(true, true, true));
    std::cout << "PASS panel cursor ownership, focus, screen and session cleanup\n";
}
