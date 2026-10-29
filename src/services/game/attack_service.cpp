#include "attack_service.hpp"
#include "anti_bot_service.hpp"

#include "../../platform/jni/jni_context.hpp"



bool services::attack::swing_main_hand(const maps::ClientPlayerEntity& player)
{
    if (!player)
        return false;
    maps::Hand hand = maps::Hand{}.MAIN_HAND.get();
    if (!hand || platform::jni_context::clear_exception())
        return false;
    player.swingHand.call(hand);
    return !platform::jni_context::clear_exception();
}


bool services::attack::attack_entity(const maps::ClientPlayerInteractionManager& manager,
    const maps::ClientPlayerEntity& player,
    const maps::Entity& target,
    bool swing)
{
    if (!manager || !player || !target)
        return false;
    if (services::anti_bot::is_bot(target)) return false;
    manager.attackEntity.call(player.toPlayerEntity(), target);
    if (platform::jni_context::clear_exception())
        return false;
    return !swing || swing_main_hand(player);
}
