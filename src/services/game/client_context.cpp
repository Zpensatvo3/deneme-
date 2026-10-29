#include "client_context.hpp"

#include "../../platform/jni/jni_context.hpp"

bool services::game::capture_client_context(client_context& output,
    bool require_world, bool require_interaction)
{
    output = {};
    if (!platform::jni_context::bind_current_thread())
        return false;

    output.instance = maps::MinecraftClient{}.instance.get();
    if (!output.instance || platform::jni_context::clear_exception())
        return false;

    output.player = output.instance.player.get();
    output.world = output.instance.world.get();
    output.interaction = output.instance.interactionManager.get();
    output.screen = output.instance.currentScreen.get();
    if (platform::jni_context::clear_exception() || !output.player)
        return false;
    if (require_world && !output.world)
        return false;
    if (require_interaction && !output.interaction)
        return false;
    return true;
}
