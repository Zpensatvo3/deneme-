#include "target_service.hpp"
#include "anti_bot_service.hpp"

#include "../../friends/friends.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../platform/adapter/game_adapter.hpp"
#include <cmath>

maps::Entity services::target::fresh_crosshair_entity(const maps::MinecraftClient& instance,
    const maps::ClientPlayerEntity& player)
{
    if (!instance || !player) return {};
    struct clear_on_exit { ~clear_on_exit() { platform::jni_context::clear_exception(); } } clear_exception;
    auto renderer = instance.gameRenderer.get();
    auto camera = instance.getCameraEntity.call();
    if (platform::jni_context::clear_exception() || !renderer || !camera ||
        !camera.is_same_object(player) || platform::jni_context::clear_exception()) return {};
    const double blocks = player.getBlockInteractionRange.call();
    const double entities = player.getEntityInteractionRange.call();
    if (platform::jni_context::clear_exception() || !std::isfinite(blocks) ||
        !std::isfinite(entities) || blocks <= 0 || entities <= 0) return {};
    // Tick delta 1 uses the latest local position, not an earlier interpolated
    // camera position. Do not overwrite vanilla's crosshairTarget or aim.
    auto hit = platform::adapter::get().crosshair_target(instance, player, camera, blocks, entities, 1.f);
    if (platform::jni_context::clear_exception() || !hit) return {};
    auto entity_type = maps::HitResult$Type{}.ENTITY.get();
    if (platform::jni_context::clear_exception() || !entity_type ||
        !hit.getType.call().is_same_object(entity_type) || platform::jni_context::clear_exception()) return {};
    auto target = maps::EntityHitResult(hit.object_instance, false).entity.get();
    return platform::jni_context::clear_exception() ? maps::Entity{} : target;
}

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
