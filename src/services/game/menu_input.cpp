#include "menu_input.hpp"
#include "menu_input_policy.hpp"
#include "../../mappings.hpp"
#include "../../platform/jni/jni_context.hpp"

namespace
{
    bool active = false;
    services::game::menu_input::cursor_lease capture;
    // Identity-only global refs; no class or member lookup at DLL load time.
    jni::object_wrapper owned_mouse{nullptr, true};
    jni::object_wrapper owned_player{nullptr, true};
    jni::object_wrapper owned_world{nullptr, true};

    void clear_mouse(maps::Mouse& mouse)
    {
        if (!mouse) return;
        mouse.leftButtonClicked.set(JNI_FALSE);
        platform::jni_context::clear_exception();
        mouse.rightButtonClicked.set(JNI_FALSE);
        platform::jni_context::clear_exception();
        mouse.cursorDeltaX.set(0.0);
        platform::jni_context::clear_exception();
        mouse.cursorDeltaY.set(0.0);
        platform::jni_context::clear_exception();
    }
}

void services::game::menu_input::update(bool open, bool focused, bool allow_restore)
{
    // GLFW owns deactivation. Do not keep rewriting its input/cursor state
    // while the user is working in a different window; retain the lease until
    // this panel resumes or closes, without ever stealing foreground focus.
    if (open && !focused) return;
    if ((!open && !active) || !platform::jni_context::bind_current_thread()) return;
    jni::frame frame{64};
    struct clear_on_exit { ~clear_on_exit() { platform::jni_context::clear_exception(); } } cleanup;
    auto instance = maps::MinecraftClient{}.instance.get();
    if (platform::jni_context::clear_exception() || !instance) return;
    auto mouse = instance.mouse.get();
    auto player = instance.player.get();
    auto world = instance.world.get();
    auto screen = instance.currentScreen.get();
    if (platform::jni_context::clear_exception() || !mouse) return;

    if (!mouse.isCursorLocked.is_valid() || !mouse.unlockCursor.is_valid() || !mouse.lockCursor.is_valid()) return;
    const bool locked = mouse.isCursorLocked.call() == JNI_TRUE;
    if (platform::jni_context::clear_exception()) return;
    if (open && !active)
    {
        owned_mouse = mouse;
        owned_player = player;
        owned_world = world;
        if (platform::jni_context::clear_exception())
        {
            owned_mouse.clear_ref();
            owned_player.clear_ref();
            owned_world.clear_ref();
            capture.invalidate();
            return;
        }
        capture.begin(locked && player && world && !screen);
        active = true;
    }
    const bool same_session = owned_mouse.is_same_object(mouse) &&
        owned_player.is_same_object(player) && owned_world.is_same_object(world);
    if (!same_session) capture.invalidate();

    // Unlike manufactured WM_KEYUP/SendInput events, vanilla unpressAll also
    // clears queued actions and covers custom bindings, sprint and mouse keys.
    maps::KeyBinding bindings{};
    const bool can_clear = bindings.unpressAll.is_valid();
    if (can_clear) bindings.unpressAll.call();
    const bool neutral_keys = !platform::jni_context::clear_exception() && can_clear;
    clear_mouse(mouse);
    if (player && neutral_keys)
    {
        auto input = player.input.get();
        if (!platform::jni_context::clear_exception() && input)
        {
            // Recalculate cached movement/jump flags from the now-neutral keys.
            // No position/velocity writes: gravity and knockback stay vanilla.
            input.tick.call();
            platform::jni_context::clear_exception();
        }
    }
    if (open)
    {
        if (locked) mouse.unlockCursor.call();
        platform::jni_context::clear_exception();
        return;
    }

    // Never steal capture from an inventory/chat screen, another window, or a
    // replacement world/player. Closing consumes the lease even when unfocused.
    if (capture.finish(focused && allow_restore, player && world && !screen, same_session) && !locked)
        mouse.lockCursor.call();
    platform::jni_context::clear_exception();
    owned_mouse.clear_ref();
    owned_player.clear_ref();
    owned_world.clear_ref();
    active = false;
}

void services::game::menu_input::shutdown()
{
    update(false, false, false);
}
