#include <Windows.h>
#include "../../cache/cache.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../services/game/hotbar_service.hpp"
#include "../../services/input/input_service.hpp"

#include <algorithm>
#include <chrono>

#include <imgui/imgui.h>

#include "pearl_macro.hpp"
#include "../../services/game/world_bridge.hpp"
#include "../../ui/settings_renderer.hpp"
#include "../ui_redirect.hpp"
#include "../../gui/gui.hpp"
#include "../../mappings.hpp"

namespace
{
	using steady_clock = std::chrono::steady_clock;
int find_pearl_slot(const maps::DefaultedList& items, const maps::Item& pearl)
	{
		if (!items || !pearl)
			return -1;
		for (int slot = 0; slot < 9; ++slot)
		{
			maps::Object object = items.get.call(static_cast<jint>(slot));
			if (!object || platform::jni_context::clear_exception())
				continue;
			maps::ItemStack stack(object.object_instance, false);
			if (!stack || stack.isEmpty.call() == JNI_TRUE || platform::jni_context::clear_exception())
				continue;
			maps::Item item = stack.item.get();
			if (item && !platform::jni_context::clear_exception() && item.is_same_object(pearl))
				return slot;
		}
		return -1;
	}
}

void modules::pearl_macro::finish(::cache& cache, bool turn_off)
{
	if (right_click_down)
	{
		services::input::mouse_button_state(services::input::mouse_button::right, false);
		right_click_down = false;
	}

	if (old_slot >= 0 && old_slot <= 8 && pearl_slot >= 0 && pearl_slot <= 8 &&
		cache.player && cache.instance)
	{
		maps::PlayerInventory inventory = cache.player.getInventory();
		if (inventory && !platform::jni_context::clear_exception() &&
			inventory.selectedSlot.get() == pearl_slot && !platform::jni_context::clear_exception() &&
			old_slot != pearl_slot)
			services::hotbar::select(cache, old_slot);
	}

	state = State::IDLE;
	old_slot = pearl_slot = -1;
	next_action = {};
	platform::jni_context::clear_exception();
	if (turn_off)
		enabled = false;
}

void modules::pearl_macro::run(::cache& cache)
{
    if constexpr (mc::target.build_id == 12111)
        if (!native_fallback_.load(std::memory_order_acquire)) return;
	const auto settings_snapshot = settings_.read();
	const HWND minecraft_window = gui::get_window();
	if (gui::draw || !minecraft_window || GetForegroundWindow() != minecraft_window ||
		!cache.is_valid() || !cache.instance || !cache.player || !jni::get_env())
	{
		finish(cache, true);
		return;
	}

	try
	{
		jni::frame frame{ 96 };
		if (cache.instance.currentScreen.get() || platform::jni_context::clear_exception())
		{
			finish(cache, true);
			return;
		}

		const auto now = steady_clock::now();
		if (state == State::IDLE)
		{
			maps::PlayerInventory inventory = cache.player.getInventory();
			maps::DefaultedList items = inventory
				? inventory.main.get()
				: maps::DefaultedList{};
			maps::Item pearl = maps::Items{}.ENDER_PEARL.get();
			if (!inventory || !items || !pearl || platform::jni_context::clear_exception())
			{
				finish(cache, true);
				return;
			}

			old_slot = inventory.selectedSlot.get();
			pearl_slot = find_pearl_slot(items, pearl);
			if (platform::jni_context::clear_exception() || old_slot < 0 || old_slot > 8 ||
				pearl_slot < 0 || !services::hotbar::select(cache, pearl_slot))
			{
				finish(cache, true);
				return;
			}
			state = State::WAIT_SWITCH;
			next_action = now + std::chrono::milliseconds(
				std::clamp(settings_snapshot.switch_delay, 0, 150));
			return;
		}

		if (now < next_action)
			return;

		if (state == State::WAIT_SWITCH)
		{
			cache.instance.itemUseCooldown.set(0);
			if (platform::jni_context::clear_exception() ||
				!services::input::mouse_button_state(services::input::mouse_button::right, true))
			{
				finish(cache, true);
				return;
			}
			right_click_down = true;
			state = State::HOLD_USE;
			// Keep the key down across at least one client tick so GLFW cannot miss it.
			next_action = now + std::chrono::milliseconds(55);
			return;
		}

		if (state == State::HOLD_USE)
		{
			if (!services::input::mouse_button_state(services::input::mouse_button::right, false))
			{
				finish(cache, true);
				return;
			}
			right_click_down = false;
			state = State::WAIT_RETURN;
			next_action = now + std::chrono::milliseconds(
				std::clamp(settings_snapshot.return_delay, 0, 250));
			return;
		}

		if (state == State::WAIT_RETURN)
			finish(cache, true);
	}
	catch (...)
	{
		platform::jni_context::clear_exception();
		finish(cache, true);
	}
}

void modules::pearl_macro::on_disable(::cache& cache)
{
    if constexpr (mc::target.build_id == 12111) {
        reset_requested_.store(true, std::memory_order_release);
        if (!native_fallback_.load(std::memory_order_acquire)) return;
    }
	finish(cache, false);
}

void modules::pearl_macro::render_overlay()
{
    if constexpr (mc::target.build_id != 12111) return;
    if (native_fallback_.load(std::memory_order_acquire) || !platform::jni_context::bind_current_thread()) return;
    namespace bridge = services::game::world_bridge;
    try {
        jni::frame frame{96};
        if (reset_requested_.exchange(false, std::memory_order_acq_rel) || !enabled) {
            if (!bridge::release_pearl()) { reset_requested_.store(true, std::memory_order_release); return; }
            state = State::IDLE; next_action = {}; if (!enabled) return;
        }
        bridge::pearl_snapshot observed;
        const auto backend = bridge::capture_pearl(observed);
        if (backend == services::game::hybrid::result::native_fallback) {
            state = State::IDLE; next_action = {}; native_fallback_.store(true, std::memory_order_release); return;
        }
        const auto stop = [&] { bridge::release_pearl(); state = State::IDLE; next_action = {}; enabled = false; };
        if (backend != services::game::hybrid::result::ready) { stop(); return; }
        const auto settings = settings_.read(); const auto now = steady_clock::now();
        if (state == State::IDLE) {
            int chosen = -1;
            for (int i = 0; i < 9; ++i) if (observed.pearls[i]) { chosen = i; break; }
            if (chosen < 0 || !bridge::begin_pearl(observed, chosen)) { stop(); return; }
            state = State::WAIT_SWITCH;
            next_action = now + std::chrono::milliseconds(std::clamp(settings.switch_delay, 0, 150)); return;
        }
        if (!observed.owned) { stop(); return; }
        if (now < next_action) return;
        if (state == State::WAIT_SWITCH) {
            if (!bridge::use_pearl()) { stop(); return; }
            state = State::HOLD_USE; next_action = now + std::chrono::milliseconds(55); return;
        }
        if (state == State::HOLD_USE) {
            state = State::WAIT_RETURN;
            next_action = now + std::chrono::milliseconds(std::clamp(settings.return_delay, 0, 250)); return;
        }
        if (state == State::WAIT_RETURN) stop();
    } catch (...) { platform::jni_context::clear_exception(); on_render_shutdown(); enabled = false; }
}

void modules::pearl_macro::on_render_shutdown()
{
    services::game::world_bridge::release_pearl();
    if (!native_fallback_.load(std::memory_order_acquire)) { state = State::IDLE; next_action = {}; }
}

void modules::pearl_macro::render_options()
{
	gui::settings::render(settings_, module_settings::pearl_macro::schema());
	ImGui::TextDisabled("%s", "One use: switch to pearl, throw, then restore the old slot.");
	ImGui::TextDisabled("%s", "Requires an ender pearl in the hotbar.");
}
