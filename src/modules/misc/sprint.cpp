#include <Windows.h>
#include "../../cache/cache.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../platform/adapter/game_adapter.hpp"

#include <imgui/imgui.h>

#include "sprint.hpp"
#include "../../ui/settings_renderer.hpp"
#include "../ui_redirect.hpp"
#include "../../gui/gui.hpp"
#include "../../mappings.hpp"
#include "../../services/game/utility_bridge.hpp"

namespace
{
}

void modules::sprint::run(::cache& cache)
{
    // 1.21.11 Java owns game I/O on the render thread. The original worker
    // implementation is retained only for older targets / rejected bindings.
    if constexpr (mc::target.build_id == 12111)
        if (!native_fallback_.load(std::memory_order_acquire)) return;
	const auto settings_snapshot = settings_.read();
	const HWND game_window = gui::get_window();
	if (gui::draw || !game_window || GetForegroundWindow() != game_window ||
		!cache.is_valid() || !cache.instance || !cache.player || !jni::get_env())
		return;

	try
	{
		jni::frame frame{ 32 };
		if (cache.instance.currentScreen.get() || platform::jni_context::clear_exception())
			return;

		if (settings_snapshot.stop_while_using_item &&
			cache.player.isUsingItem.call() == JNI_TRUE && !platform::jni_context::clear_exception())
			return;
		platform::jni_context::clear_exception();

		if (!platform::adapter::get().can_start_sprinting(cache.player, cache.options))
			return;

		if (cache.player.isSprinting.call() != JNI_TRUE && !platform::jni_context::clear_exception())
		{
			cache.player.setSprinting.call(JNI_TRUE);
			if (!platform::jni_context::clear_exception())
				started_by_module = true;
		}
	}
	catch (...)
	{
		platform::jni_context::clear_exception();
	}
}

void modules::sprint::on_disable(::cache& cache)
{
    if constexpr (mc::target.build_id == 12111)
    {
        reset_requested_.store(true, std::memory_order_release);
        if (!native_fallback_.load(std::memory_order_acquire)) return;
    }
	if (!started_by_module)
		return;

	try
	{
		if (cache.player && jni::get_env() &&
			cache.player.isSprinting.call() == JNI_TRUE && !platform::jni_context::clear_exception())
			cache.player.setSprinting.call(JNI_FALSE);
		platform::jni_context::clear_exception();
	}
	catch (...)
	{
		platform::jni_context::clear_exception();
	}
	started_by_module = false;
}

void modules::sprint::render_overlay()
{
    if constexpr (mc::target.build_id != 12111) return;
    if (native_fallback_.load(std::memory_order_acquire) || !platform::jni_context::bind_current_thread()) return;
    if (reset_requested_.exchange(false, std::memory_order_acq_rel) &&
        !services::game::utility::release(services::game::utility::feature::sprint))
    {
        reset_requested_.store(true, std::memory_order_release);
        return;
    }
    const auto settings = settings_.read();
    const auto result = services::game::utility::update(services::game::utility::feature::sprint,
        enabled, settings.stop_while_using_item ? 1 : 0);
    if (result == services::game::hybrid::result::native_fallback)
        native_fallback_.store(true, std::memory_order_release);
}

void modules::sprint::on_render_shutdown()
{
    if constexpr (mc::target.build_id == 12111)
        services::game::utility::release(services::game::utility::feature::sprint);
}

void modules::sprint::render_options()
{
	gui::settings::render(settings_, module_settings::sprint::schema());
}
