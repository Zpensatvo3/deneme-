#include "../src/modules/misc/pot_refill_engine.hpp"
#include <cassert>
#include <iostream>
#include <set>
#include <vector>

int main()
{
    using namespace modules::pot_refill_logic;
    using namespace std::chrono_literals;
    std::mt19937_64 random{42};
    // Shuffled handler layout and remapped screen IDs, plus excluded/offhand slots.
    std::vector<slot> slots{
        {35, 97, false, true, 1}, {8, 203, true, false, 0},
        {0, 101, true, false, 0}, {9, 51, false, true, 1},
        {1, 102, false, true, 1}, {10, 52, false, false, 1},
        {11, 53, false, true, 0}, {40, 40, false, true, 1},
        {-1, 0, true, false, 0}, {12, -1, false, true, 1}
    };
    assert((select(slots, 0, random) == move{9, 51, 0, 101}));
    assert((select(slots, 1, random) == move{35, 97, 8, 203}));
    std::set<int> sources, targets;
    for (int i = 0; i < 200; ++i)
    {
        const auto action = select(slots, 2, random);
        assert(action);
        assert(action->source_index == 9 || action->source_index == 35);
        assert(action->target_index == 0 || action->target_index == 8);
        sources.insert(action->source_index);
        targets.insert(action->target_index);
    }
    assert(sources.size() == 2 && targets.size() == 2);
    auto ambiguous = slots;
    ambiguous.push_back({0, 999, true, false, 0});
    assert(!select(ambiguous, 0, random));
    auto duplicate_id = slots;
    duplicate_id.push_back({2, 101, true, false, 0});
    assert(!select(duplicate_id, 0, random));
    auto full = slots;
    for (auto& slot : full) if (slot.inventory_index < 9) slot.empty = false;
    assert(!select(full, 0, random)); // Never overwrite occupied hotbar slots.
    auto no_potions = slots;
    for (auto& slot : no_potions) slot.healing = false;
    assert(!select(no_potions, 0, random));
    assert(!select({}, 0, random));

    module_settings::pot_refill settings;
    for (int i = 0; i < 100; ++i) assert(delay(settings, random) == 100ms);
    settings.random_delay = true;
    for (int i = 0; i < 100; ++i)
    {
        const auto duration = delay(settings, random);
        assert(duration >= 100ms && duration <= 160ms);
    }
    settings.delay_ms = 700; settings.max_delay_ms = 50;
    assert(delay(settings, random) == 700ms);
    settings.delay_ms = -500; settings.max_delay_ms = 9999;
    for (int i = 0; i < 100; ++i)
    {
        const auto duration = delay(settings, random);
        assert(duration >= 50ms && duration <= 1000ms);
    }

    engine engine;
    const auto now = clock::time_point{} + 10s;
    const move action{9, 51, 0, 101};
    engine.arm(now, 100ms);
    assert(!engine.ready(now + 99ms));
    assert(!engine.submitted(action, now + 99ms));
    assert(engine.submitted(action, now + 100ms));
    assert(!engine.ready(now + 110ms));
    assert(!engine.submitted(action, now + 110ms)); // No duplicate packets while waiting.
    assert(engine.observe(now + 100ms, true) == observation::waiting);
    assert(engine.observe(now + 110ms, false) == observation::waiting);
    assert(engine.observe(now + 120ms, true) == observation::completed);
    assert(!engine.pending());
    engine.arm(now + 120ms, 100ms);
    assert(!engine.ready(now + 219ms) && engine.ready(now + 220ms));
    assert(engine.submitted(action, now + 220ms));
    assert(engine.observe(now + 1719ms, false) == observation::waiting);
    assert(engine.observe(now + 1720ms, false) == observation::timed_out);
    assert(engine.blocked() && !engine.pending() && !engine.ready(now + 60s));
    assert(!engine.submitted(action, now + 60s));
    assert(engine.observe(now + 60s, true) == observation::idle); // Late state cannot revive it.

    // Disable, focus/cursor/manual input, config and screen/world changes all
    // call reset in the JNI adapter. Pending work is abandoned, never reversed.
    for (int reason = 0; reason < 6; ++reason)
    {
        engine.reset();
        engine.arm(now, 100ms);
        assert(engine.submitted(action, now + 100ms));
        engine.reset();
        assert(!engine.pending() && !engine.blocked());
        engine.arm(now + 101ms, 100ms);
        assert(!engine.ready(now + 200ms));
        assert(engine.observe(now + 202ms, true) == observation::idle);
    }
    engine.stop();
    assert(engine.blocked() && !engine.ready(now + 60s));
    std::cout << "PotRefill selection, pacing, verification, timeout and reset checks passed\n";
}
