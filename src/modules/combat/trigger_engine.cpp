#include "trigger_engine.hpp"
#include <algorithm>
#include <cmath>

bool trigger_engine::normalize(config& value)
{
    if (!std::isfinite(value.cooldown[0]) || !std::isfinite(value.cooldown[1]) ||
        static_cast<int>(value.targets) < 0 || static_cast<int>(value.targets) > 2 ||
        static_cast<int>(value.weapon) < 0 || static_cast<int>(value.weapon) > 2 ||
        static_cast<int>(value.criticals) < 0 || static_cast<int>(value.criticals) > 2) return false;
    if (value.cooldown[0] > value.cooldown[1]) std::swap(value.cooldown[0], value.cooldown[1]);
    if (value.reaction[0] > value.reaction[1]) std::swap(value.reaction[0], value.reaction[1]);
    for (auto& item : value.cooldown) item = std::clamp(item, .5f, 1.f);
    for (auto& item : value.reaction) item = std::clamp(item, 0, 150);
    return true;
}
bool trigger_engine::same_config(const config& a, const config& b)
{
    return a.cooldown[0] == b.cooldown[0] && a.cooldown[1] == b.cooldown[1] &&
        a.reaction[0] == b.reaction[0] && a.reaction[1] == b.reaction[1] &&
        a.targets == b.targets && a.weapon == b.weapon && a.criticals == b.criticals && a.on_click == b.on_click && a.swing_hand == b.swing_hand &&
        a.click_simulation == b.click_simulation;
}
bool trigger_engine::weapon_allowed(weapon_type held, weapon_mode mode)
{
    return mode == weapon_mode::any || (mode == weapon_mode::sword && held == weapon_type::sword) ||
        (mode == weapon_mode::axe && held == weapon_type::axe);
}
void trigger_engine::reset(runtime& state) { state = {}; }
void trigger_engine::invalidate(runtime& state, std::int64_t now)
{
    auto backoff = state.backoff_until;
    if (state.ticket)
        backoff = std::max(backoff, std::max(now, state.confirmation_deadline) + 250);
    reset(state);
    state.backoff_until = backoff;
    if (now < backoff) state.status = phase::backoff;
}
void trigger_engine::begin_cycle(const config& settings, runtime& state, std::int64_t now, sampler& random)
{
    state.threshold = std::clamp(random.cooldown(settings.cooldown[0], settings.cooldown[1]), settings.cooldown[0], settings.cooldown[1]);
    state.reaction_ready = now + std::clamp(random.reaction(settings.reaction[0], settings.reaction[1]), settings.reaction[0], settings.reaction[1]);
    state.critical_started = -1; state.status = phase::tracking;
}
void trigger_engine::submission_failed(runtime& state, std::int64_t now)
{
    state.ticket = 0; state.pending_attack_ticks = -1;
    state.backoff_until = now + 250; state.critical_started = -1; state.status = phase::backoff;
}
void trigger_engine::submitted(runtime& state, std::uint64_t ticket, const observation& frame, std::int64_t now)
{
    state.ticket = ticket; state.pending_attack_ticks = frame.attack_ticks;
    state.confirmation_deadline = now + 180; state.manual_generation = frame.manual_generation;
    state.last_attack_ticks = frame.attack_ticks; state.last_cooldown = frame.cooldown;
    state.status = phase::confirmation;
}
trigger_engine::action trigger_engine::step(const config& settings, runtime& state,
    const observation& frame, std::int64_t now, sampler& random)
{
    const bool had_ticket = state.ticket != 0;
    const bool valid = frame.eligible && frame.weapon_permitted && frame.target_id >= 0 &&
        frame.session != 0 && frame.attack_ticks >= 0 && std::isfinite(frame.cooldown) &&
        (!settings.on_click || frame.physical_left);
    if (!valid) {
        invalidate(state, now);
        return had_ticket ? action::cancel : action::none;
    }
    const bool changed = state.target_id != frame.target_id || state.session != frame.session ||
        state.settings_revision != frame.settings_revision || state.weapon_revision != frame.weapon_revision;
    if (changed) {
        invalidate(state, now);
        state.target_id = frame.target_id; state.session = frame.session;
        state.settings_revision = frame.settings_revision; state.weapon_revision = frame.weapon_revision;
        state.manual_generation = frame.manual_generation;
        state.last_attack_ticks = frame.attack_ticks; state.last_cooldown = frame.cooldown;
        if (now < state.backoff_until) state.status = phase::backoff;
        else begin_cycle(settings, state, now, random);
        if (had_ticket) return action::cancel;
    }
    const bool manual = frame.manual_generation != state.manual_generation;
    const bool consumed = (state.last_attack_ticks >= 0 && frame.attack_ticks < state.last_attack_ticks) ||
        (state.last_cooldown >= .5f && frame.cooldown < .25f);
    state.last_attack_ticks = frame.attack_ticks; state.last_cooldown = frame.cooldown;
    state.manual_generation = frame.manual_generation;
    if (state.ticket) {
        if (manual || frame.input_cancelled) { invalidate(state, now); return action::cancel; }
        if (frame.input_delivered && ((state.pending_attack_ticks >= 0 && frame.attack_ticks < state.pending_attack_ticks) || frame.cooldown < .25f)) {
            state.ticket = 0; state.pending_attack_ticks = -1;
            begin_cycle(settings, state, now, random);
            return action::retire; // Retire the ticket; never catch up with a burst.
        }
        if (now >= state.confirmation_deadline) { submission_failed(state, now); return action::cancel; }
        state.status = phase::confirmation; return action::none;
    }
    if (manual || consumed) {
        // A late vanilla attack/manual click must not skip the fresh reaction
        // required after an interrupted automatic request.
        if (now < state.backoff_until) state.status = phase::backoff;
        else begin_cycle(settings, state, now, random);
        return action::cancel;
    }
    if (now < state.backoff_until) { state.status = phase::backoff; return action::none; }
    if (state.status == phase::backoff) { begin_cycle(settings, state, now, random); return action::none; }
    if (now < state.reaction_ready) { state.status = phase::reaction; state.critical_started = -1; return action::none; }
    if (frame.cooldown < state.threshold) { state.status = phase::cooldown; state.critical_started = -1; return action::none; }
    if (settings.criticals == critical_mode::only && !frame.critical_valid) { state.status = phase::critical; return action::none; }
    if (settings.criticals == critical_mode::smart && !frame.on_ground && frame.critical_possible && !frame.critical_valid) {
        if (state.critical_started < 0) state.critical_started = now;
        if (now - state.critical_started < 150) { state.status = phase::critical; return action::none; }
    }
    state.status = phase::preparing; return action::prepare;
}
