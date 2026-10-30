#include "target_service.hpp"
#include "anti_bot_service.hpp"

#include "../../friends/friends.hpp"
#include "../../platform/jni/jni_context.hpp"


maps::Entity services::target::crosshair_entity(const maps::MinecraftClient& instance)
{
    if (!instance)
        return {};

    maps::HitResult result = instance.crosshairTarget.get();
    maps::HitResult$Type entity_type = maps::HitResult$Type{}.ENTITY.get();
    if (!result || !entity_type || platform::jni_context::clear_exception() ||
        !result.getType.call().is_same_object(entity_type) ||
        platform::jni_context::clear_exception())
        return {};

    maps::Entity entity = maps::EntityHitResult(result.object_instance, false).entity.get();
    return platform::jni_context::clear_exception() ? maps::Entity{} : entity;
}

maps::Entity services::target::living_crosshair_target(
    const maps::MinecraftClient& instance, bool ignore_friends)
{
    maps::Entity entity = crosshair_entity(instance);
    if (!entity || !entity.is_instance_of<maps::LivingEntity>() ||
        entity.isAlive.call() != JNI_TRUE || platform::jni_context::clear_exception())
        return {};
    if (ignore_friends && entity.is_instance_of<maps::PlayerEntity>() && friends::is_friend(entity))
        return {};
    if (services::anti_bot::is_bot(entity)) return {};
    return entity;
}

maps::Entity services::target::blocking_player_crosshair_target(
    const maps::MinecraftClient& instance,
    const maps::ClientPlayerEntity& player,
    double maximum_distance,
    bool ignore_friends)
{
    maps::Entity target = living_crosshair_target(instance, ignore_friends);
    if (!target || !player || !target.is_instance_of<maps::PlayerEntity>())
        return {};

    maps::PlayerEntity target_player(target.object_instance, false);
    if (target_player.isBlocking.call() != JNI_TRUE || platform::jni_context::clear_exception())
        return {};

    const double distance_squared = player.squaredDistanceTo.call(target);
    if (platform::jni_context::clear_exception() ||
        distance_squared > maximum_distance * maximum_distance)
        return {};
    return target;
}
