#pragma once

#include "../../mappings.hpp"

class cache;

namespace services::hotbar
{
    bool valid_slot(int slot);
    int selected_slot(const maps::PlayerInventory& inventory);

    // Changes the local selection and immediately uses Minecraft's own slot
    // synchronizer so client and server state remain in the same transaction.
    bool select(maps::PlayerInventory inventory,
        const maps::ClientPlayerInteractionManager& manager, int slot);
    bool select(const maps::ClientPlayerEntity& player,
        const maps::ClientPlayerInteractionManager& manager, int slot);
    bool select(::cache& cache, int slot);

    // Restores only while the module still owns the selected slot. A manual
    // player slot change therefore never gets overwritten by delayed cleanup.
    bool restore_if_owned(const maps::ClientPlayerEntity& player,
        const maps::ClientPlayerInteractionManager& manager,
        int previous_slot, int owned_slot);
}
