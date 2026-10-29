#include "../src/modules/combat/trigger_engine.hpp"
#include "../src/services/input/click_tracking.hpp"
#include <cassert>
#include <cmath>
#include <limits>
#include <cstdio>
#include <initializer_list>

using namespace trigger_engine;
struct fixed_sampler final : sampler
{
    int cooldown_calls = 0, reaction_calls = 0;
    float cooldown(float a, float) override { ++cooldown_calls; return a; }
    int reaction(int a, int) override { ++reaction_calls; return a; }
};
static observation valid_frame()
{
    observation f; f.eligible = f.weapon_permitted = true;
    f.target_id = 10; f.session = 1; f.cooldown = 1; f.attack_ticks = 20; return f;
}
int main()
{
    config settings; settings.criticals = critical_mode::off;
    fixed_sampler random; runtime state; auto f = valid_frame();
    assert(step(settings, state, f, 0, random) == action::none && state.status == phase::reaction);
    for (int ms = 1; ms < 25; ++ms) assert(step(settings, state, f, ms, random) == action::none);
    assert(random.cooldown_calls == 1 && random.reaction_calls == 1);
    assert(step(settings, state, f, 25, random) == action::prepare);
    submitted(state, 1, f, 25);
    assert(step(settings, state, f, 100, random) == action::none && state.ticket == 1);
    f.input_delivered = true; f.cooldown = .1f; f.attack_ticks = 0;
    assert(step(settings, state, f, 110, random) == action::retire && state.ticket == 0);
    assert(random.cooldown_calls == 2 && random.reaction_calls == 2);
    assert(step(settings, state, f, 200, random) == action::none && state.status == phase::cooldown);
    f.cooldown = 1; f.attack_ticks = 20;
    assert(step(settings, state, f, 300, random) == action::prepare);

    // No local consumption before the ticket was delivered; no blind resend.
    reset(state); random = {}; f = valid_frame();
    step(settings, state, f, 0, random); submitted(state, 2, f, 25);
    f.cooldown = .1f; f.attack_ticks = 0;
    assert(step(settings, state, f, 30, random) == action::none && state.ticket == 2);
    assert(step(settings, state, f, 205, random) == action::cancel && state.status == phase::backoff);
    f = valid_frame();
    for (int ms : {206, 250, 400, 454}) assert(step(settings, state, f, ms, random) == action::none);
    assert(step(settings, state, f, 455, random) == action::none); // Fresh reaction starts now.
    assert(step(settings, state, f, 479, random) == action::none);
    assert(step(settings, state, f, 480, random) == action::prepare);
    submitted(state, 3, f, 480);
    assert(step(settings, state, f, 100000, random) == action::cancel); // No catch-up burst.

    // Knockback can remove the crosshair target for just one frame. Cancelling
    // queued input is not proof that Minecraft did not consume it afterwards.
    // All interruption paths must retain the original ACK window + backoff.
    for (int cause = 0; cause < 8; ++cause) {
        reset(state); random = {}; f = valid_frame();
        step(settings, state, f, 0, random); submitted(state, 6, f, 25);
        auto interrupted = f;
        switch (cause) {
        case 0: interrupted.eligible = false; break;
        case 1: ++interrupted.target_id; break;
        case 2: ++interrupted.session; break;
        case 3: ++interrupted.settings_revision; break;
        case 4: ++interrupted.weapon_revision; break;
        case 5: invalidate(state, 30); break; // Adapter capture/disable/reset path.
        case 6: interrupted.input_cancelled = true; break; // Delivery guard rejected the target.
        case 7: ++interrupted.manual_generation; break;
        }
        if (cause != 5) assert(step(settings, state, interrupted, 30, random) == action::cancel);
        assert(state.ticket == 0 && state.backoff_until == 455);
        for (int ms = 31; ms < 455; ++ms) {
            // Repeated target loss and late manual input during backoff cannot
            // clear the guard or prematurely start the next reaction interval.
            f.eligible = ms % 3 != 0;
            if (ms == 440) ++f.manual_generation;
            assert(step(settings, state, f, ms, random) != action::prepare);
            assert(state.backoff_until == 455);
        }
        f.eligible = true;
        assert(step(settings, state, f, 455, random) == action::none);
        assert(state.reaction_ready == 480);
        assert(step(settings, state, f, 479, random) == action::none);
        assert(step(settings, state, f, 480, random) == action::prepare);
    }

    // A delayed interruption still waits at least 250 ms from cancellation.
    reset(state); f = valid_frame();
    step(settings, state, f, 0, random); submitted(state, 7, f, 25);
    invalidate(state, 1000); assert(state.backoff_until == 1250);
    invalidate(state, 1100); assert(state.backoff_until == 1250);

    // Damage/airborne critical-state changes alone must not restart an already
    // submitted attack or treat a delivered edge as another completed attack.
    reset(state); random = {}; f = valid_frame();
    step(settings, state, f, 0, random); submitted(state, 8, f, 25);
    f.input_delivered = true; f.on_ground = false; f.critical_possible = true;
    for (int ms = 30; ms <= 100; ++ms) {
        f.critical_valid = ms % 2 != 0;
        assert(step(settings, state, f, ms, random) == action::none);
        assert(state.ticket == 8 && state.status == phase::confirmation);
    }
    assert(random.cooldown_calls == 1 && random.reaction_calls == 1);
    f.cooldown = .1f; f.attack_ticks = 0;
    assert(step(settings, state, f, 110, random) == action::retire);
    assert(step(settings, state, f, 180, random) == action::none);

    // Integrate delivery rejection with the previous knockback retry guard:
    // even if the target reappears next frame, there is no immediate resend.
    reset(state); random = {}; f = valid_frame();
    services::input::click_ledger delivery;
    const auto delivery_id = delivery.reserve(false);
    step(settings, state, f, 0, random); submitted(state, delivery_id, f, 25);
    assert(!delivery.observe(services::input::ticket_tag(delivery_id, true), true,
        services::input::event_path::window, false, false));
    const auto receipt = delivery.receipt(delivery_id);
    f.input_cancelled = receipt.cancelled; f.input_delivered = receipt.delivered;
    assert(step(settings, state, f, 30, random) == action::cancel);
    assert(state.backoff_until == 455 && state.ticket == 0);
    f.input_cancelled = false;
    assert(step(settings, state, f, 31, random) == action::none);
    assert(step(settings, state, f, 454, random) == action::none);
    assert(step(settings, state, f, 455, random) == action::none && state.reaction_ready == 480);
    assert(step(settings, state, f, 479, random) == action::none);
    assert(step(settings, state, f, 480, random) == action::prepare);

    // Smart's 150 ms budget starts only after both timing gates are satisfied.
    reset(state); f = valid_frame(); settings.criticals = critical_mode::smart;
    f.on_ground = false; f.critical_possible = true; f.cooldown = .2f;
    assert(step(settings, state, f, 0, random) == action::none);
    assert(step(settings, state, f, 300, random) == action::none && state.critical_started == -1);
    f.cooldown = 1;
    assert(step(settings, state, f, 301, random) == action::none && state.status == phase::critical);
    assert(step(settings, state, f, 450, random) == action::none);
    assert(step(settings, state, f, 451, random) == action::prepare);
    f.critical_possible = false; assert(step(settings, state, f, 452, random) == action::prepare);
    settings.criticals = critical_mode::only;
    assert(step(settings, state, f, 1000, random) == action::none);
    f.critical_valid = true; assert(step(settings, state, f, 1001, random) == action::prepare);
    settings.criticals = critical_mode::smart; f.critical_valid = false; f.on_ground = true;
    assert(step(settings, state, f, 1002, random) == action::prepare);

    // Every invalidation cancels the one in-flight ticket and reacquires timing.
    settings.criticals = critical_mode::off;
    for (int cause = 0; cause < 7; ++cause) {
        reset(state); f = valid_frame(); step(settings, state, f, 0, random); submitted(state, 4, f, 25);
        auto changed = f;
        switch (cause) {
        case 0: changed.eligible = false; break;
        case 1: ++changed.target_id; break;
        case 2: ++changed.session; break;
        case 3: ++changed.settings_revision; break;
        case 4: ++changed.weapon_revision; break;
        case 5: ++changed.manual_generation; break;
        case 6: changed.input_cancelled = true; break;
        }
        assert(step(settings, state, changed, 30, random) == action::cancel && state.ticket == 0);
    }
    reset(state); f = valid_frame(); settings.on_click = true;
    assert(step(settings, state, f, 0, random) == action::none && state.status == phase::idle);
    f.physical_left = true; assert(step(settings, state, f, 1, random) == action::none);
    assert(step(settings, state, f, 26, random) == action::prepare);
    submitted(state, 5, f, 26); f.physical_left = false;
    assert(step(settings, state, f, 27, random) == action::cancel);
    settings.on_click = false;

    for (int increment : {1, 16, 50, 100}) {
        reset(state); f = valid_frame(); fixed_sampler fps_random;
        for (int ms = 0; ms <= 1000; ms += increment) step(settings, state, f, ms, fps_random);
        assert(fps_random.cooldown_calls == 1 && fps_random.reaction_calls == 1);
    }
    config invalid; invalid.cooldown[0] = std::numeric_limits<float>::quiet_NaN(); assert(!normalize(invalid));
    invalid = {}; invalid.weapon = static_cast<weapon_mode>(9); assert(!normalize(invalid));
    invalid = {}; invalid.cooldown[0] = 1.2f; invalid.cooldown[1] = .1f;
    invalid.reaction[0] = 200; invalid.reaction[1] = -20;
    assert(normalize(invalid) && invalid.cooldown[0] == .5f && invalid.cooldown[1] == 1.f && invalid.reaction[0] == 0 && invalid.reaction[1] == 150);
    assert(weapon_allowed(weapon_type::sword, weapon_mode::sword));
    assert(!weapon_allowed(weapon_type::mace, weapon_mode::sword));
    std::puts("Trigger model timing, cancellation, confirmation, critical and normalization scenarios passed.");
}
