#include "../src/modules/combat/anti_bot_engine.hpp"
#include <cassert>
#include <iostream>

int main()
{
    using anti_bot_engine::runtime;
    module_settings::anti_bot config;
    runtime state;
    static_assert(module_settings::anti_bot_default_enabled);
    for (int age = 0; age < 40; ++age) assert(!state.observe(config, "a:1", age, true, false, age * 50));
    assert(!state.observe(config, "a:1", 40, true, false, 2000));
    for (int i = 0; i < 200; ++i) assert(!state.observe(config, "a:1", 40, true, false, 2000));
    for (int age = 41; age < 80; ++age) assert(!state.observe(config, "a:1", age, true, false, age * 50));
    assert(state.observe(config, "a:1", 80, true, false, 4000));
    assert(!state.observe(config, "a:1", 81, true, true, 4050)); // Real entry arrives: unfilter immediately.
    assert(state.size() == 0);
    assert(!state.observe(config, "a:1", 82, true, false, 4100)); // Missing again requires a fresh streak.
    assert(!state.observe(config, "a:1", 83, false, false, 4150)); // Own entry/JNI data unreliable.
    assert(state.size() == 0);
    assert(!state.observe(config, "a:1", 84, true, false, 4200));
    assert(!state.observe(config, "a:2", 124, true, false, 6200)); // New entity ID can't inherit UUID's streak.
    assert(state.observe(config, "a:1", 124, true, false, 6200));
    state.clear(); // Disconnect, world/respawn, disable or config change.
    assert(!state.observe(config, "a:1", 125, true, false, 6250));
    assert(!state.observe(config, "a:1", 40, true, false, 6300)); // Age rollback/reused object.
    assert(!state.observe(config, "a:1", 200, true, false, 14300)); // Long unobserved interval.
    assert(!state.observe(config, "a:1", 201, true, false, 14000)); // Clock rollback is conservative.
    assert(!state.observe(config, "a:1", -1, true, false, 14050));
    config.check_player_list = false;
    assert(!state.observe(config, "a:1", 500, true, false, 15000) && state.size() == 0);
    config.check_player_list = true;
    for (int i = 0; i < 5000; ++i)
        assert(!state.observe(config, "uuid:" + std::to_string(i), 100, true, false, 16000));
    assert(state.size() <= 4096);
    assert(!state.observe(config, "fresh:1", 100, true, false, 30000));
    assert(state.size() == 1); // Departed records expire instead of growing forever.
    std::cout << "AntiBot: spawn grace, age-based confirmation, late Tab entries, unknown data, session resets and bounded cache passed\n";
}
