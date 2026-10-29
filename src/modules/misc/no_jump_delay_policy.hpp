#pragma once

namespace no_jump_delay_policy
{
    struct observation
    {
        bool enabled = false;
        bool focused = false;
        bool screen_open = false;
        bool alive = false;
        int cooldown = 0;
    };

    constexpr bool should_clear(const observation& state)
    {
        return state.enabled && state.focused && !state.screen_open &&
            state.alive && state.cooldown > 0;
    }
}
