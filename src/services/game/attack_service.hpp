#pragma once

#include "../../mappings.hpp"

namespace services::attack
{
    bool swing_main_hand(const maps::ClientPlayerEntity& player);
    bool attack_entity(const maps::ClientPlayerInteractionManager& manager,
        const maps::ClientPlayerEntity& player,
        const maps::Entity& target,
        bool swing = true);
}
