#include "../src/modules/combat/jump_reset_engine.hpp"
#include "../src/services/input/jump_key_tracking.hpp"
#include <cassert>
#include <cstdio>
#include <initializer_list>

using namespace jump_reset_engine;
struct fixed_roll final : sampler
{
    int value = 1, calls = 0;
    int percent() override { ++calls; return value; }
};
static observation valid_frame()
{
    observation f; f.eligible = f.on_ground = true; f.age = 100; f.session = f.revision = 1; return f;
}
int main()
{
    jump_reset_config::config settings;
    runtime state; fixed_roll random; auto f = valid_frame();
    assert(step(settings, state, f, 0, random) == action::none);
    f.hurt = 10; f.living_damage = true;
    assert(step(settings, state, f, 1, random) == action::press);
    submitted(state, f.age, 1);
    f.delivered = true;
    assert(step(settings, state, f, 25, random) == action::none && state.holding);
    ++f.age;
    assert(step(settings, state, f, 50, random) == action::release && !state.holding);
    for (int now : {51, 100, 141, 200}) assert(step(settings, state, f, now, random) != action::press);
    f.hurt = 9; step(settings, state, f, 201, random);
    f.hurt = 10; assert(step(settings, state, f, 202, random) == action::press);

    // A tick before native delivery is not evidence the DOWN was consumed.
    state = {}; f = valid_frame(); step(settings, state, f, 0, random);
    f.hurt = 10; f.living_damage = true; step(settings, state, f, 1, random); submitted(state, f.age, 1);
    ++f.age; assert(step(settings, state, f, 50, random) == action::none);
    f.delivered = true; assert(step(settings, state, f, 60, random) == action::none);
    ++f.age; assert(step(settings, state, f, 100, random) == action::release);
    state = {}; f = valid_frame(); step(settings, state, f, 0, random);
    f.hurt = 10; f.living_damage = true; step(settings, state, f, 1, random); submitted(state, f.age, 1, true);
    ++f.age; assert(step(settings, state, f, 50, random) == action::release);

    // Failed chance/expired confirmation never rerolls one hurt animation.
    state = {}; f = valid_frame(); random = {}; random.value = 51; settings.chance = 50;
    step(settings, state, f, 0, random); f.hurt = 10; f.living_damage = true;
    assert(step(settings, state, f, 1, random) == action::none && random.calls == 1);
    for (int now = 2; now < 170; ++now) assert(step(settings, state, f, now, random) == action::none);
    assert(random.calls == 1);
    settings = {}; state = {}; f = valid_frame(); step(settings, state, f, 0, random);
    f.hurt = 10; assert(step(settings, state, f, 1, random) == action::none && state.status == phase::confirmation);
    assert(step(settings, state, f, 105, random) == action::none);
    assert(step(settings, state, f, 106, random) == action::none && !state.pending);
    f.living_damage = true; assert(step(settings, state, f, 107, random) == action::none);
    state = {}; f = valid_frame(); step(settings, state, f, 0, random);
    f.hurt = 10; step(settings, state, f, 1, random);
    f.living_damage = true; assert(step(settings, state, f, 110, random) == action::none && !state.pending);

    // A bounded delayed source can confirm the pending hit, never a later one.
    state = {}; f = valid_frame(); step(settings, state, f, 0, random); f.hurt = 10;
    step(settings, state, f, 1, random); f.living_damage = true;
    assert(step(settings, state, f, 51, random) == action::press);
    submission_failed(state, 51);
    assert(step(settings, state, f, 500, random) == action::none);

    // Ground grace permits tracking a fresh hit, not sending an airborne jump.
    state = {}; f = valid_frame(); settings.jump_delay_ms = 40;
    step(settings, state, f, 0, random); f.hurt = 10; f.living_damage = true;
    assert(step(settings, state, f, 1, random) == action::none && state.status == phase::delay);
    f.on_ground = false;
    assert(step(settings, state, f, 45, random) == action::none && state.status == phase::ground);
    assert(step(settings, state, f, 172, random) == action::none && !state.pending);
    f.on_ground = true; assert(step(settings, state, f, 173, random) == action::none);

    state = {}; f = valid_frame(); settings.jump_delay_ms = 0;
    step(settings, state, f, 0, random); f.hurt = 10; f.living_damage = true; f.jump_ready = false;
    assert(step(settings, state, f, 1, random) == action::none && state.pending);
    f.jump_ready = true; assert(step(settings, state, f, 50, random) == action::press);

    // Settings/session/focus changes and real input release the one owned hold.
    settings = {};
    for (int reason = 0; reason < 6; ++reason) {
        state = {}; f = valid_frame(); step(settings, state, f, 0, random);
        f.hurt = 10; f.living_damage = true; step(settings, state, f, 1, random); submitted(state, f.age, 1);
        switch (reason) {
        case 0: f.eligible = false; break;
        case 1: ++f.session; break;
        case 2: ++f.revision; break;
        case 3: f.physical_jump = true; break;
        case 4: f.input_cancelled = true; break;
        case 5: settings.chance = 0; break;
        }
        assert(step(settings, state, f, 20, random) == action::release && !state.holding);
        settings.chance = 100;
    }
    state = {}; f = valid_frame(); step(settings, state, f, 0, random);
    f.hurt = 10; f.living_damage = true; step(settings, state, f, 1, random); submitted(state, f.age, 1);
    assert(step(settings, state, f, 201, random) == action::release && state.timed_out);
    assert(step(settings, state, f, 202, random) != action::press);

    using namespace services::input;
    jump_key_ledger keys;
    const auto id = keys.reserve(); assert(id && !keys.reserve());
    assert(keys.observe(jump_key_event_tag(id, true), true, false));
    assert(keys.delivered && !keys.observe(jump_key_event_tag(id, true), true, false));
    keys.cancel(id); assert(!keys.reserve()); // Await key-up cleanup before another request.
    assert(!keys.observe(jump_key_event_tag(id, false), false, true)); // Real hold wins.
    keys.up_queued = true;
    const auto next = keys.reserve(); assert(next != id);
    assert(!keys.observe(jump_key_event_tag(id, true), true, false));
    assert(!keys.observe(jump_key_event_tag(id, false), false, false));
    assert(!keys.observe(jump_key_event_tag(next, true), true, false, false) && keys.cancelled);
    keys.up_queued = true; keys.next = 0x7fffffu; assert(!keys.reserve());
    assert(windows_key_for_glfw(32) == 32 && windows_key_for_glfw(71) == 71);
    assert(windows_key_for_glfw(340) == 0xa0 && windows_key_for_glfw(344) == 0xa1);
    assert(windows_key_for_glfw(-1) == 0);
    std::puts("Jump decision timing, one-roll damage edges, cancellation, actual-ground gating, key ownership and stale events passed.");
}
