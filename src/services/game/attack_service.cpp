#include "attack_service.hpp"
#include "anti_bot_service.hpp"
#include "client_context.hpp"

#include "../../platform/jni/jni_context.hpp"
#include <cmath>

float services::attack::cooldown(const maps::ClientPlayerEntity& player, float tick_delta)
{
    float value = 0;
    return read_cooldown(player, value, tick_delta) ? value : 0.f;
}

bool services::attack::read_cooldown(const maps::ClientPlayerEntity& player, float& output, float tick_delta)
{
    output = 0;
    if (!player) return false;
    const float value = player.getAttackCooldownProgress.call(tick_delta);
    if (platform::jni_context::clear_exception() || !std::isfinite(value)) return false;
    output = value; return true;
}

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

bool services::attack::restore_left_hold(bool pressed)
{
    if (!platform::jni_context::bind_current_thread()) return false;
    jni::frame frame{16};
    services::game::client_context context;
    if (!services::game::capture_client_context(context, false, false)) return false;
    auto options = context.instance.options.get();
    auto mouse = context.instance.mouse.get();
    if (platform::jni_context::clear_exception() || !options || !mouse) return false;
    auto attack_key = options.attackKey.get();
    if (platform::jni_context::clear_exception() || !attack_key) return false;
    const jboolean held = pressed && !context.screen ? JNI_TRUE : JNI_FALSE;
    // Unlike Mouse.onMouseButton, setPressed does not increment timesPressed.
    // Existing manual presses are neither consumed nor cleared here.
    attack_key.setPressed.call(held);
    mouse.leftButtonClicked.set(held);
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
