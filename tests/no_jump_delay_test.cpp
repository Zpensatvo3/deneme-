#include "../src/modules/misc/no_jump_delay_policy.hpp"
#include <cassert>
#include <iostream>

int main()
{
    using namespace no_jump_delay_policy;
    // Exercise every combination of activation/focus/screen/life guards.
    for (const bool enabled : {false, true})
        for (const bool focused : {false, true})
            for (const bool screen_open : {false, true})
                for (const bool alive : {false, true})
                    for (const int cooldown : {-1, 0, 1, 10})
                    {
                        observation state{enabled, focused, screen_open, alive, cooldown};
                        const bool expected = enabled && focused && !screen_open && alive && cooldown > 0;
                        assert(should_clear(state) == expected);
                    }

    observation current{true, true, false, true, 10};
    assert(should_clear(current));
    current.cooldown = 0;
    assert(!should_clear(current)); // No redundant write after clearing.
    current.cooldown = 10;
    current.enabled = false;
    assert(!should_clear(current)); // Disabling leaves vanilla cooldown alone.
    std::cout << "NoJumpDelay eligibility checks passed\n";
}
