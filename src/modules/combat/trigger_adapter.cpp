#include "trigger_adapter.hpp"
#include "../../services/game/world_bridge.hpp"
#include "../../services/game/attack_coordinator.hpp"

void trigger_engine::request_reset(adapter_runtime& state)
{
    std::lock_guard lock(state.submission_mutex);
    state.reset_generation.fetch_add(1, std::memory_order_release);
    services::combat::release_trigger();
}
void trigger_engine::run_overlay(const config& settings, adapter_runtime& state, bool enabled)
{
    std::lock_guard lock(state.submission_mutex);
    services::game::world_bridge::tick_trigger(settings, enabled, state.reset_generation);
}
