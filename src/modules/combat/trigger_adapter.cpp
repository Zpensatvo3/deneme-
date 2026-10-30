#include "trigger_adapter.hpp"
#include "../../services/game/world_bridge.hpp"
#include "../../services/game/attack_coordinator.hpp"

void trigger_engine::request_reset(adapter_runtime& state)
{
    state.reset_generation.fetch_add(1, std::memory_order_release);
    services::combat::release_trigger();
}
void trigger_engine::run_overlay(adapter_runtime& state, bool enabled)
{
    services::game::world_bridge::tick_trigger(enabled, state.reset_generation);
}
