#pragma once
// Legacy native decision model retained as an offline migration reference.
// Live Jump Reset execution is owned by WorldBridge.java feature 5.
#include "jump_reset_config.hpp"
#include <cstdint>

namespace jump_reset_engine
{
    enum class phase { idle, confirmation, delay, ground, holding, cooldown };
    enum class action { none, press, release };
    struct observation
    {
        bool eligible = false, on_ground = false, physical_jump = false;
        bool living_damage = false, delivered = false, input_cancelled = false, jump_ready = true;
        int hurt = 0, age = 0;
        std::uint64_t session = 0, revision = 0;
    };
    struct runtime
    {
        phase status = phase::idle;
        bool seeded = false, pending = false, holding = false, confirmed = false, timed_out = false;
        bool delivery_seen = false;
        int previous_hurt = 0, sent_age = 0;
        std::uint64_t session = 0, revision = 0, damage_id = 0;
        std::int64_t last_grounded = -1, confirm_until = 0, ready_at = 0, expires_at = 0;
        std::int64_t hold_until = 0, next_allowed = 0;
    };
    struct sampler { virtual ~sampler() = default; virtual int percent() = 0; };
    inline void invalidate(runtime& state)
    {
        const auto backoff = state.next_allowed;
        state = {}; state.next_allowed = backoff;
    }
    inline void submitted(runtime& state, int age, std::int64_t now, bool immediate_delivery = false)
    {
        state.pending = false; state.holding = true; state.sent_age = age;
        state.delivery_seen = immediate_delivery;
        state.hold_until = now + 200; state.next_allowed = now + 140;
        state.status = phase::holding;
    }
    inline void submission_failed(runtime& state, std::int64_t now)
    {
        state.pending = state.holding = false;
        state.next_allowed = now + 140; state.status = phase::cooldown;
    }
    inline action step(const jump_reset_config::config& settings, runtime& state,
        const observation& frame, std::int64_t now, sampler& random)
    {
        const bool had_hold = state.holding;
        if (!frame.eligible || frame.session == 0 || frame.age < 0 || frame.hurt < 0 || settings.chance == 0) {
            invalidate(state); return had_hold ? action::release : action::none;
        }
        if (!state.seeded || state.session != frame.session || state.revision != frame.revision) {
            invalidate(state); state.seeded = true;
            state.session = frame.session; state.revision = frame.revision;
            state.previous_hurt = frame.hurt; state.last_grounded = frame.on_ground ? now : -1;
            return had_hold ? action::release : action::none;
        }
        const bool damaged = frame.hurt > 0 && frame.hurt > state.previous_hurt;
        state.previous_hurt = frame.hurt;
        if (frame.on_ground) state.last_grounded = now;
        if (state.holding) {
            // Native delivery may lag submission by a tick. Observe its first
            // delivery age before waiting for a subsequent game update.
            if (frame.delivered && !state.delivery_seen) {
                state.delivery_seen = true; state.sent_age = frame.age;
            }
            if (frame.physical_jump || frame.input_cancelled || (state.delivery_seen && frame.age > state.sent_age) ||
                now >= state.hold_until || frame.age < state.sent_age) {
                state.timed_out = now >= state.hold_until && !frame.physical_jump;
                state.holding = false; state.status = phase::cooldown;
                return action::release;
            }
            state.status = phase::holding; return action::none;
        }
        if (frame.physical_jump) { state.pending = false; state.status = phase::idle; return action::none; }
        if (damaged) {
            // A newer hit supersedes an older pending decision. Roll only once
            // for this rising damage edge; expiry/failure cannot retry that edge.
            state.pending = false; ++state.damage_id; state.timed_out = false;
            const bool grounded_recently = frame.on_ground || (state.last_grounded >= 0 && now - state.last_grounded <= 85);
            if (now >= state.next_allowed && grounded_recently &&
                (settings.chance == 100 || random.percent() <= settings.chance)) {
                state.pending = true; state.confirmed = !settings.only_combat || frame.living_damage;
                state.confirm_until = now + 105; state.ready_at = now + settings.jump_delay_ms;
                state.expires_at = now + 170;
            }
        }
        if (!state.pending) {
            state.status = now < state.next_allowed ? phase::cooldown : phase::idle; return action::none;
        }
        if (now >= state.expires_at || frame.hurt == 0) { state.pending = false; state.status = phase::idle; return action::none; }
        if (!state.confirmed) {
            if (now >= state.confirm_until) { state.pending = false; state.status = phase::idle; return action::none; }
            state.confirmed = frame.living_damage;
            if (!state.confirmed) {
                state.status = phase::confirmation;
                return action::none;
            }
        }
        if (now < state.ready_at) { state.status = phase::delay; return action::none; }
        // Ground grace is for detecting a hit, not permission to jump midair.
        if (!frame.on_ground || !frame.jump_ready) { state.status = phase::ground; return action::none; }
        return action::press;
    }
}
