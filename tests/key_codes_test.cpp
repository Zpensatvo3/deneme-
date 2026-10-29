#include "../src/platform/input/key_codes.hpp"
#include "../src/modules/core/module_runtime_state.hpp"
#include <cassert>
#include <set>
#include <iostream>

int main()
{
    using namespace platform::keys;
    std::set<int> seen;
    for (const auto& key : keyboard) {
        assert(seen.insert(key.code).second && valid(key.code));
        assert(key.x >= 0 && key.y >= 0 && key.w > 0 && key.h > 0 && key.x + key.w <= 23 && key.y + key.h <= 6.5f);
        for (const auto& other : keyboard) {
            if (&key == &other) continue;
            assert(key.x + key.w <= other.x || other.x + other.w <= key.x ||
                key.y + key.h <= other.y || other.y + other.h <= key.y);
        }
    }
    assert(seen.size() == 104);
    for (int f = 0x70; f <= 0x7b; ++f) assert(find(f));
    for (int n = 0; n <= 9; ++n) {
        assert(find('0' + n) && find(0x60 + n));
        assert(from_message(0x60+n,scan_for(0x60+n)) == 0x60+n);
    }
    assert(from_message(0x2d,0x52) == 0x60); // NumLock off: keypad 0, not Insert.
    assert(from_message(0x2d,0x152) == 0x2d);
    assert(from_message(0x24,0x47) == 0x67 && from_message(0x24,0x147) == 0x24);
    assert(from_message(0x0d,0x1c) == 0x0d && from_message(0x0d,0x11c) == num_enter);
    assert(scan_for(0x0d) == 0x1c && scan_for(num_enter) == 0x11c);
    assert(from_message(0x10,0x2a) == 0xa0 && from_message(0x10,0x36) == 0xa1);
    assert(from_message(0x11,0x1d) == 0xa2 && from_message(0x11,0x11d) == 0xa3);
    assert(from_message(0x12,0x38) == 0xa4 && from_message(0x12,0x138) == 0xa5);
    for (int reserved : {0x1b,0xa1,0x23,0x5b,0x5c}) assert(!assignable(reserved) && reserved_reason(reserved));
    assert(assignable('A') && assignable(num_enter));
    assert(valid(255) && valid(num_enter) && !valid(256) && !valid(-1) && !valid(270));
    assert(label(num_enter) == "Num Enter" && label(0x60) == "Num 0" && label(0x70) == "F1");
    modules::key_edge edge;
    assert(!edge.sample('A',false,false));
    assert(!edge.sample('A',true,false)); // UI typing is sampled, not executed.
    assert(!edge.sample('A',true,true)); // Held through close: no extra action.
    assert(!edge.sample(num_enter,true,true)); // Rebinding while held is not a press.
    assert(!edge.sample(num_enter,false,true));
    assert(edge.sample(num_enter,true,true));
    std::cout << "104 unique non-overlapping keys, VK/scan mapping, Numpad identity, reserved keys and input edges passed\n";
}
