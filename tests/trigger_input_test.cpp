#include "../src/services/game/attack_coordinator.hpp"
#include <cassert>
#include <cstdio>

int main()
{
    int attempts = 0;
    services::combat::release_trigger();
    assert(services::combat::try_unowned_attack([&] { ++attempts; return true; }));
    services::combat::claim_trigger(10, 1);
    assert(services::combat::trigger_owns_attack());
    assert(!services::combat::try_unowned_attack([&] { ++attempts; return true; }));
    assert(attempts == 1);
    services::combat::release_trigger();
    assert(services::combat::try_unowned_attack([&] { ++attempts; return true; }) && attempts == 2);
    std::puts("Attack ownership and native conflict coordination passed.");
}
