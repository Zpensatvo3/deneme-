#pragma once

#include "../../sdk/minecraft/common/inventory_network.hpp"

namespace services::inventory
{
    enum class hotbar_item_kind
    {
        sword,
        axe
    };

    bool slot_matches(const maps::DefaultedList& items, int slot, hotbar_item_kind kind);
    int find_hotbar_slot(const maps::DefaultedList& items,
        hotbar_item_kind kind, int preferred_slot = -1);
}
