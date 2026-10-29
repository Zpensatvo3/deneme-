#include <imgui/imgui.h>
#include "../../cache/cache.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../services/game/client_context.hpp"
#include "../../services/game/utility_bridge.hpp"
#include "../../services/input/input_service.hpp"
#include <algorithm>
#include "fast_place.hpp"
#include "../../ui/settings_renderer.hpp"
#include "../ui_redirect.hpp"
#include "../../gui/gui.hpp"

namespace
{
bool is_supported_fast_place_stack(const maps::ItemStack& stack)
	{
		if (!stack || stack.isEmpty.call() == JNI_TRUE || platform::jni_context::clear_exception())
			return false;

		maps::Item item = stack.item.get();
		if (!item || platform::jni_context::clear_exception())
			return false;

		const bool supported = item.is_instance_of<maps::BlockItem>() ||
			item.is_instance_of<maps::PotionItem>();
		if (platform::jni_context::clear_exception())
			return false;
		return supported;
	}

	bool is_supported_fast_place_item(const maps::ClientPlayerEntity& player)
	{
		maps::ItemStack main_hand = player.getMainHandStack.call();
		maps::ItemStack off_hand = player.getOffHandStack.call();
		if (platform::jni_context::clear_exception())
			return false;
		return is_supported_fast_place_stack(main_hand) ||
			is_supported_fast_place_stack(off_hand);
	}
}

void modules::fast_place::run(::cache&)
{
    // Cooldown changes run alongside vanilla input on the client thread.
}

void modules::fast_place::render_overlay()
{
	const auto settings_snapshot = settings_.read();
    const HWND window = gui::get_window();
    if (!enabled || gui::draw || gui::loader_visible || gui::should_unload || !window ||
        GetForegroundWindow() != window || !platform::jni_context::bind_current_thread()) return;
    struct clear_on_exit { ~clear_on_exit() { platform::jni_context::clear_exception(); } } cleanup;
	try {
        jni::frame frame{48};
        const auto backend = services::game::utility::update(services::game::utility::feature::fast_place,
            enabled, std::clamp(settings_snapshot.delay, 0, 4),
            services::input::physical_button_down(services::input::mouse_button::right));
        if (backend != services::game::hybrid::result::native_fallback) return;
        services::game::client_context context;
        if (!services::game::capture_client_context(context, true, false) || context.screen ||
            context.player.isAlive.call() != JNI_TRUE || platform::jni_context::clear_exception()) return;
        auto options = context.instance.options.get();
        if (platform::jni_context::clear_exception() || !options) return;
        auto use_key = options.useKey.get();
        if (platform::jni_context::clear_exception() || !use_key || !use_key.isPressed.is_valid()) return;
        const bool held = use_key.isPressed.call() == JNI_TRUE ||
            services::input::physical_button_down(services::input::mouse_button::right);
        if (platform::jni_context::clear_exception() || !held ||
            !is_supported_fast_place_item(context.player) || !context.instance.itemUseCooldown.is_valid()) return;
        const int current_cooldown = context.instance.itemUseCooldown.get();
        const int desired_delay = std::clamp(settings_snapshot.delay, 0, 4);
        // PotionItem includes splash/lingering potions. Reduce only vanilla's
        // click delay; do not duplicate use calls or bypass drinking duration.
        if (!platform::jni_context::clear_exception() && current_cooldown > desired_delay)
            context.instance.itemUseCooldown.set(desired_delay);
	}
	catch (...) { platform::jni_context::clear_exception(); }
}

void modules::fast_place::on_disable(::cache& cache)
{
}

void modules::fast_place::render_options()
{
	gui::settings::render(settings_, module_settings::fast_place::schema());
}
