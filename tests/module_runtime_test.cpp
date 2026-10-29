#include "../src/modules/core/module_runtime_state.hpp"
#include <cassert>
#include <iostream>
#include <string>

namespace
{
    struct context { std::string events; };
    struct fake_module
    {
        bool enabled = false, prev_enabled = false;
        bool disable_on_enable = false, disable_on_run = false;
        void on_enable(context& c) { c.events += 'E'; if (disable_on_enable) enabled = false; }
        void on_disable(context& c) { c.events += 'D'; }
        void run(context& c) { c.events += 'R'; if (disable_on_run) enabled = false; }
    };
}

int main()
{
    modules::key_edge key;
    assert(!key.sample(65, false, true));
    assert(key.sample(65, true, true));
    assert(!key.sample(65, true, true));
    assert(!key.sample(65, false, true));
    // Typing/alt-tab holds are sampled but must not toggle on focus return.
    assert(!key.sample(65, true, false));
    assert(!key.sample(65, true, true));
    assert(!key.sample(65, false, true));
    assert(key.sample(65, true, true));
    // Capturing a new binding while it is held is not a fresh gameplay press.
    assert(!key.sample(66, true, true));
    assert(!key.sample(66, false, true));
    assert(key.sample(66, true, true));
    assert(!key.sample(0, true, true));
    assert(!key.sample(66, true, true));
    modules::key_edge initial;
    assert(initial.sample(65, true, true));

    context c;
    fake_module entry;
    modules::tick_module(entry, c);
    assert(c.events.empty());
    entry.enabled = true;
    modules::tick_module(entry, c);
    modules::tick_module(entry, c);
    assert(c.events == "ERR");
    entry.enabled = false;
    modules::tick_module(entry, c);
    modules::tick_module(entry, c);
    assert(c.events == "ERRD");

    c.events.clear();
    entry.enabled = true;
    entry.disable_on_run = true;
    modules::tick_module(entry, c);
    assert(c.events == "ERD" && !entry.prev_enabled);
    modules::stop_module(entry, c);
    assert(c.events == "ERD");

    c.events.clear();
    entry.enabled = true;
    entry.disable_on_enable = true;
    modules::tick_module(entry, c);
    assert(c.events == "ED");

    c.events.clear();
    entry.enabled = false;
    entry.prev_enabled = true; // Panic cleared enabled before worker cleanup.
    modules::stop_module(entry, c);
    modules::stop_module(entry, c);
    assert(c.events == "D" && !entry.enabled && !entry.prev_enabled);
    std::cout << "Module runtime input/lifecycle tests passed\n";
}
