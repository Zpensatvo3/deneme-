#include "anti_bot_service.hpp"
#include "client_context.hpp"
#include "overlay_context.hpp"
#include "../../modules/combat/anti_bot_engine.hpp"
#include "../../platform/jni/jni_context.hpp"
#include <atomic>
#include <chrono>
#include <mutex>

namespace
{
    std::atomic_bool active{false};
    std::mutex mutex;
    module_settings::anti_bot config;
    anti_bot_engine::runtime decisions;
    services::game::overlay_session session;
    void clear_locked()
    {
        decisions.clear();
        if (jni::get_env()) session.reset(); else session.invalidate();
    }
}

void services::anti_bot::configure(bool enabled, const module_settings::anti_bot& settings)
{
    std::lock_guard lock(mutex);
    if (active.load(std::memory_order_relaxed) == enabled && config == settings) return;
    config = settings; clear_locked();
    active.store(enabled, std::memory_order_release);
}

void services::anti_bot::clear_session()
{
    std::lock_guard lock(mutex);
    clear_locked();
}

bool services::anti_bot::is_bot(const maps::Entity& entity)
{
    if (!active.load(std::memory_order_acquire) || !entity || !platform::jni_context::bind_current_thread()) return false;
    struct clear_on_exit { ~clear_on_exit() { platform::jni_context::clear_exception(); } } cleanup;
    jni::frame frame{64};
    try
    {
        if (!entity.is_instance_of<maps::PlayerEntity>() || platform::jni_context::clear_exception()) return false;
        services::game::client_context context;
        if (!services::game::capture_client_context(context, true, false)) { clear_session(); return false; }
        if (entity.is_same_object(context.player)) return false;
        auto entity_world = entity.getWorld.call();
        if (platform::jni_context::clear_exception() || !entity_world || !entity_world.is_same_object(context.world)) return false;
        auto network = context.instance.getNetworkHandler.call();
        maps::PlayerEntity player(entity.object_instance, false);
        auto profile = player.getGameProfile.call();
        auto uuid = profile ? profile.getId.call() : maps::UUID{};
        auto local_profile = context.player.getGameProfile.call();
        auto local_uuid = local_profile ? local_profile.getId.call() : maps::UUID{};
        if (platform::jni_context::clear_exception() || !network || !uuid || !local_uuid) { clear_session(); return false; }
        // Even unlisted (hidden-from-Tab) real players have network entries.
        // If our own entry is missing, the whole list is not reliable yet.
        auto local_entry = network.getPlayerListEntry.call(local_uuid);
        auto entry = network.getPlayerListEntry.call(uuid);
        auto text = uuid.toString.call();
        const int entity_id = entity.getId.call(), age = entity.age.get();
        if (platform::jni_context::clear_exception() || !text) { clear_session(); return false; }
        const std::string identity = text.to_string() + ":" + std::to_string(entity_id);
        if (platform::jni_context::clear_exception()) { clear_session(); return false; }
        const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        std::lock_guard lock(mutex);
        if (!active.load(std::memory_order_relaxed)) return false;
        if (session.observe({context.world, context.player})) decisions.clear();
        return decisions.observe(config, identity, age, static_cast<bool>(local_entry), static_cast<bool>(entry), now);
    }
    catch (...) { platform::jni_context::clear_exception(); clear_session(); return false; }
}
