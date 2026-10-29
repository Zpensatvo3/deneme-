#pragma once

#include "../../mappings.hpp"

namespace services::attack
{
    float cooldown(const maps::ClientPlayerEntity& player, float tick_delta = 0.5f);
    bool read_cooldown(const maps::ClientPlayerEntity& player, float& output, float tick_delta = 0.5f);
    bool swing_main_hand(const maps::ClientPlayerEntity& player);
    // Restore a physical hold without queuing a fresh attack-key press.
    bool restore_left_hold(bool pressed);
    bool attack_entity(const maps::ClientPlayerInteractionManager& manager,
        const maps::ClientPlayerEntity& player,
        const maps::Entity& target,
        bool swing = true);
}
