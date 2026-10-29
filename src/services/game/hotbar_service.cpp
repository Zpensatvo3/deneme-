#include "hotbar_service.hpp"

#include "../../platform/jni/jni_context.hpp"
#include "../../cache/cache.hpp"

bool services::hotbar::valid_slot(int slot)
{
    return slot >= 0 && slot <= 8;
}

int services::hotbar::selected_slot(const maps::PlayerInventory& inventory)
{
    if (!inventory)
        return -1;

    const int selected = static_cast<int>(inventory.selectedSlot.get());
    return platform::jni_context::clear_exception() || !valid_slot(selected)
        ? -1
        : selected;
}

bool services::hotbar::select(maps::PlayerInventory inventory,
    const maps::ClientPlayerInteractionManager& manager, int slot)
{
    if (!inventory || !manager || !valid_slot(slot))
        return false;

    const int current = selected_slot(inventory);
    if (current < 0)
        return false;
    if (current == slot)
        return true;

    inventory.selectedSlot.set(static_cast<jint>(slot));
    if (platform::jni_context::clear_exception())
        return false;

    manager.syncSelectedSlot.call();
    return !platform::jni_context::clear_exception();
}

bool services::hotbar::select(const maps::ClientPlayerEntity& player,
    const maps::ClientPlayerInteractionManager& manager, int slot)
{
    if (!player)
        return false;

    maps::PlayerInventory inventory = player.getInventory.call();
    return !platform::jni_context::clear_exception() && select(inventory, manager, slot);
}

bool services::hotbar::select(::cache& cache, int slot)
{
    if (!cache.instance || !cache.player)
        return false;

    maps::ClientPlayerInteractionManager manager = cache.instance.interactionManager.get();
    return !platform::jni_context::clear_exception() && select(cache.player, manager, slot);
}

bool services::hotbar::restore_if_owned(const maps::ClientPlayerEntity& player,
    const maps::ClientPlayerInteractionManager& manager,
    int previous_slot, int owned_slot)
{
    if (!valid_slot(previous_slot) || !valid_slot(owned_slot) || !player)
        return false;

    maps::PlayerInventory inventory = player.getInventory.call();
    if (platform::jni_context::clear_exception())
        return false;

    const int current = selected_slot(inventory);
    if (current < 0)
        return false;
    return current != owned_slot || select(inventory, manager, previous_slot);
}
