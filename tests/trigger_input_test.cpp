#include "../src/services/input/click_tracking.hpp"
#include "../src/services/game/attack_coordinator.hpp"
#include <cassert>
#include <cstdio>

int main()
{
    using namespace services::input;
    assert(classify_submission(0) == submission_status::rejected);
    assert(classify_submission(1) == submission_status::partial);
    assert(classify_submission(2) == submission_status::complete);
    click_ledger ledger;
    const auto id = ledger.reserve(false); assert(id && !ledger.reserve(false));
    assert(tracked_tag(ticket_tag(id, true)));
    assert(ledger.observe(ticket_tag(id, true), true, event_path::window, false));
    assert(!ledger.receipt(id).delivered);
    assert(!ledger.observe(ticket_tag(id, true), true, event_path::window, false));
    assert(ledger.observe(ticket_tag(id, false), false, event_path::window, false));
    assert(ledger.receipt(id).delivered);
    assert(ledger.observe(ticket_tag(id, true), true, event_path::raw, false));
    assert(ledger.observe(ticket_tag(id, false), false, event_path::raw, false));
    assert(ledger.receipt(id).delivered); // Two paths remain one identity.
    ledger.cancel(id); assert(ledger.receipt(id).cancelled);
    assert(!ledger.observe(ticket_tag(id, true), true, event_path::window, false));
    const auto next = ledger.reserve(true); assert(next != id);
    assert(!ledger.observe(ticket_tag(id, false), false, event_path::window, false));
    assert(ledger.observe(ticket_tag(next, false), false, event_path::window, true));
    ledger.cancel(next);
    assert(!ledger.observe(ticket_tag(next, false), false, event_path::raw, true));
    assert(!ledger.observe(ticket_tag(next, true), true, event_path::window, true));
    ledger.next = 0x7fffffu; assert(!ledger.reserve(false));

    // Fast target exit is rejected before either edge enters the game.
    click_ledger guarded;
    auto guarded_id = guarded.reserve(false);
    assert(guarded.needs_validation(ticket_tag(guarded_id, true), true, event_path::window));
    assert(!guarded.observe(ticket_tag(guarded_id, true), true, event_path::window, false, false));
    assert(guarded.receipt(guarded_id).cancelled && !guarded.receipt(guarded_id).delivered);
    assert(guarded.window_edges == 0 && !guarded.needs_repair());
    assert(!guarded.observe(ticket_tag(guarded_id, true), true, event_path::raw, false));

    // Preserving a real physical hold must validate BEFORE the initial UP.
    guarded_id = guarded.reserve(true);
    assert(guarded.needs_validation(ticket_tag(guarded_id, false), false, event_path::window));
    assert(!guarded.observe(ticket_tag(guarded_id, false), false, event_path::window, true, false));
    assert(guarded.window_edges == 0 && !guarded.needs_repair());

    // If the target exits between UP and DOWN, no DOWN reaches vanilla. Hold
    // restoration has a separate tag and must not count as attack delivery.
    guarded_id = guarded.reserve(true);
    assert(guarded.observe(ticket_tag(guarded_id, false), false, event_path::window, true));
    assert(guarded.needs_repair());
    assert(guarded.needs_validation(ticket_tag(guarded_id, true), true, event_path::window));
    assert(!guarded.observe(ticket_tag(guarded_id, true), true, event_path::window, true, false));
    assert(guarded.receipt(guarded_id).cancelled && !guarded.receipt(guarded_id).delivered);
    const auto repair_tag = restore_ticket_tag(guarded_id, true);
    assert(restore_tag(repair_tag) && !tracked_tag(repair_tag));
    assert(guarded.accepts_restore(repair_tag));
    assert(!guarded.observe(ticket_tag(guarded_id, true), true, event_path::raw, true));
    const auto newer_id = guarded.reserve(false);
    assert(newer_id != guarded_id && !guarded.accepts_restore(repair_tag));
    assert(!guarded.needs_validation(ticket_tag(guarded_id, true), true, event_path::window));
    assert(guarded.observe(ticket_tag(newer_id, true), true, event_path::window, false));
    assert(!guarded.needs_validation(ticket_tag(newer_id, true), true, event_path::window));
    assert(!guarded.needs_validation(ticket_tag(newer_id, false), false, event_path::window));
    assert(guarded.observe(ticket_tag(newer_id, false), false, event_path::window, false));
    assert(guarded.receipt(newer_id).delivered);
    assert(!guarded.needs_validation(ticket_tag(newer_id, true), true, event_path::raw));
    // A finished transport is already delivered; another transport must not
    // restart its timing/validation or attribute its edges to a new request.
    int attempts = 0;
    services::combat::release_trigger();
    assert(services::combat::try_unowned_attack([&] { ++attempts; return true; }));
    services::combat::claim_trigger(10, 1);
    assert(services::combat::trigger_owns_attack());
    assert(!services::combat::try_unowned_attack([&] { ++attempts; return true; }));
    assert(attempts == 1);
    services::combat::release_trigger();
    assert(services::combat::try_unowned_attack([&] { ++attempts; return true; }) && attempts == 2);
    std::puts("Input identity, fast-target delivery rejection, hold restoration, stale events and attack ownership passed.");
}
