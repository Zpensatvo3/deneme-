#include <Windows.h>
#include "../../cache/cache.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../services/game/hotbar_service.hpp"
#include "../../services/game/client_context.hpp"
#include "../../services/game/inventory_service.hpp"
#include "../../services/game/target_service.hpp"
#include "../../services/game/attack_service.hpp"
#include "../../services/game/attack_coordinator.hpp"

#include <algorithm>
#include <chrono>
#include <string>

#include <imgui/imgui.h>

#include "shield_breaker.hpp"
#include "../../ui/settings_renderer.hpp"
#include "../ui_redirect.hpp"
#include "../../gui/gui.hpp"
#include "../../gui/widgets.hpp"
#include "../../mappings.hpp"


namespace
{
}

bool modules::shield_breaker::prepare_current_thread()
{
    auto settings_snapshot = settings_.read();
	if (gui::draw || !gui::runtime_ready.load(std::memory_order_acquire))
		return false;

	services::game::client_context context{};
	if (!services::game::capture_client_context(context, true, true))
		return false;
	auto& instance = context.instance;
	auto& player = context.player;
	auto& manager = context.interaction;
	if (waiting_to_restore)
	{
		const int selected = services::hotbar::selected_slot(player.getInventory.call());
		if (selected == axe_slot && trigger_world && trigger_world.is_same_object(context.world) &&
			trigger_player.is_same_object(player) && !platform::jni_context::clear_exception()) return true;
		// A manual slot change or a new world invalidates the old lease.
		old_slot = -1; axe_slot = -1; waiting_to_restore = false; trigger_prepared = false;
		trigger_world.clear_ref(); trigger_player.clear_ref();
	}
	const HWND window = gui::get_window();
	if (!window || GetForegroundWindow() != window)
		return false;

	if (context.screen)
		return false;

	if (!services::target::blocking_player_crosshair_target(instance, player))
		return false;

	maps::PlayerInventory inventory = player.getInventory.call();
	maps::DefaultedList items = inventory ? inventory.main.get() : maps::DefaultedList{};
	const int selected_slot = inventory ? static_cast<int>(inventory.selectedSlot.get()) : -1;
	if (!inventory || !items || selected_slot < 0 || selected_slot > 8 || platform::jni_context::clear_exception())
		return false;

	const int selected_axe_slot = services::inventory::find_hotbar_slot(items, services::inventory::hotbar_item_kind::axe, settings_snapshot.config_axe_slot - 1);
	if (selected_axe_slot < 0 || platform::jni_context::clear_exception())
		return false;

	// Already holding an axe: keep the existing slot for this click.
	if (selected_slot == selected_axe_slot)
		return true;

	old_slot = selected_slot;
	axe_slot = selected_axe_slot;
	if (!services::hotbar::select(player, manager, axe_slot))
	{
		old_slot = -1;
		axe_slot = -1;
		return false;
	}

	switched_at = std::chrono::steady_clock::now();
	waiting_to_restore = true;
	trigger_world = context.world; trigger_player = player;
	return true;
}

bool modules::shield_breaker::break_current_target()
{
	if (gui::draw || !gui::runtime_ready.load(std::memory_order_acquire))
		return false;

	services::game::client_context context{};
	if (!services::game::capture_client_context(context, false, true))
		return false;
	auto& instance = context.instance;
	auto& player = context.player;
	auto& manager = context.interaction;

	maps::Entity target = services::target::blocking_player_crosshair_target(instance, player);
	if (!target)
	{
		// A new shield raise by the same player must be eligible immediately.
		last_break_target_id = -1;
		return false;
	}

	const int target_id = static_cast<int>(target.getId.call());
	if (platform::jni_context::clear_exception())
		return false;

	const auto now = std::chrono::steady_clock::now();
	if (target_id == last_break_target_id &&
		now - last_break_at < std::chrono::milliseconds(350))
		return false;

	if (!prepare_current_thread())
		return false;

	// Validate again after the slot synchronization. This closes the tiny
	// frame window in which the crosshair or shield state could have changed.
	target = services::target::blocking_player_crosshair_target(instance, player);
	if (!target || static_cast<int>(target.getId.call()) != target_id || platform::jni_context::clear_exception())
		return false;

	if (!services::attack::attack_entity(manager, player, target))
		return false;

	last_break_target_id = target_id;
	last_break_at = now;
	return true;
}

void modules::shield_breaker::render_overlay()
{
	// render_overlay executes on Minecraft's render/client thread. Performing
	// the detection, slot sync and attack here removes all dependency on a
	// physical click and avoids unsafe interaction-manager calls from workers.
	if (!enabled || !platform::jni_context::bind_current_thread())
		return;

	std::lock_guard lock(state_mutex);
	jni::frame frame{ 160 };
	try
	{
		services::combat::try_unowned_attack([&] { return break_current_target(); });
	}
	catch (...)
	{
		platform::jni_context::clear_exception();
	}
}

bool modules::shield_breaker::gate_left_click()
{
	shield_breaker* instance = active_instance.load(std::memory_order_acquire);
	if (!instance || !instance->enabled || !platform::jni_context::bind_current_thread())
		return false;

	std::lock_guard lock(instance->state_mutex);
	jni::frame frame{ 128 };
	try
	{
		const bool prepared = instance->prepare_current_thread();
		if (prepared && instance->trigger_prepared)
		{
			// A real click takes over this preparation; Triggerbot must not roll
			// the slot back before Minecraft consumes the user's click.
			instance->trigger_prepared = false;
			instance->switched_at = std::chrono::steady_clock::now();
		}
		return prepared;
	}
	catch (...)
	{
		platform::jni_context::clear_exception();
		return false;
	}
}


bool modules::shield_breaker::prepare_for_trigger_attack()
{
    auto* instance = active_instance.load(std::memory_order_acquire);
    if (!instance || !instance->enabled || !platform::jni_context::bind_current_thread()) return false;
    std::lock_guard lock(instance->state_mutex);
    jni::frame frame{128};
    try {
        if (!instance->prepare_current_thread()) return false;
        if (instance->waiting_to_restore) {
            services::game::client_context context{};
            if (!services::game::capture_client_context(context, true, true)) {
                instance->old_slot = -1; instance->axe_slot = -1; instance->waiting_to_restore = false;
                return false;
            }
            instance->trigger_world = context.world; instance->trigger_player = context.player;
            instance->trigger_prepared = true;
        }
        return !platform::jni_context::clear_exception();
    } catch (...) { platform::jni_context::clear_exception(); return false; }
}
int modules::shield_breaker::trigger_prepared_slot()
{
    auto* instance = active_instance.load(std::memory_order_acquire);
    if (!instance) return -1;
    std::lock_guard lock(instance->state_mutex);
    return instance->enabled && instance->trigger_prepared ? instance->axe_slot : -1;
}
int modules::shield_breaker::starting_slot_if_owned(const maps::ClientPlayerEntity& player,
    const maps::ClientWorld& world, int selected)
{
    auto* instance = active_instance.load(std::memory_order_acquire);
    if (!instance) return -1;
    std::lock_guard lock(instance->state_mutex);
    if (!instance->enabled || !instance->waiting_to_restore || selected != instance->axe_slot ||
        !instance->trigger_world || !instance->trigger_player.is_same_object(player) ||
        !instance->trigger_world.is_same_object(world)) return -1;
    return instance->old_slot;
}

void modules::shield_breaker::finish_trigger_preparation(const maps::ClientPlayerEntity& player,
    const maps::ClientPlayerInteractionManager& manager, bool consumed)
{
    auto* instance = active_instance.load(std::memory_order_acquire);
    if (!instance) return;
    std::lock_guard lock(instance->state_mutex);
    if (!instance->trigger_prepared) return;
    instance->trigger_prepared = false;
    if (consumed && instance->enabled) {
        instance->switched_at = std::chrono::steady_clock::now();
        return;
    }
    if (player && manager && instance->trigger_player.is_same_object(player))
        services::hotbar::restore_if_owned(player, manager, instance->old_slot, instance->axe_slot);
    instance->old_slot = -1; instance->axe_slot = -1; instance->waiting_to_restore = false;
    instance->trigger_world.clear_ref(); instance->trigger_player.clear_ref();
    platform::jni_context::clear_exception();
}

void modules::shield_breaker::restore(::cache& cache)
{
	if (trigger_world && (!cache.instance || !cache.player || !trigger_player.is_same_object(cache.player) ||
		!trigger_world.is_same_object(cache.instance.world.get())))
	{
		old_slot = -1; axe_slot = -1; waiting_to_restore = false; trigger_prepared = false;
		trigger_world.clear_ref(); trigger_player.clear_ref(); return;
	}
	if (waiting_to_restore && old_slot >= 0 && old_slot <= 8 &&
		cache.player && cache.instance)
	{
		try
		{
			maps::PlayerInventory inventory = cache.player.getInventory.call();
			maps::ClientPlayerInteractionManager manager = cache.instance.interactionManager.get();
			if (inventory && manager && !platform::jni_context::clear_exception())
			{
				const int selected = static_cast<int>(inventory.selectedSlot.get());
				if (!platform::jni_context::clear_exception() && selected == axe_slot)
					services::hotbar::select(cache.player, manager, old_slot);
			}
		}
		catch (...)
		{
			platform::jni_context::clear_exception();
		}
	}

	old_slot = -1;
	axe_slot = -1;
	waiting_to_restore = false;
	trigger_prepared = false;
	trigger_world.clear_ref(); trigger_player.clear_ref();
	switched_at = {};
}

void modules::shield_breaker::run(::cache& cache)
{
    auto settings_snapshot = settings_.read();
	std::lock_guard lock(state_mutex);
	if (trigger_prepared) return; // Client-thread Triggerbot owns cancellation/commit.
	if (!waiting_to_restore)
		return;

	const auto now = std::chrono::steady_clock::now();
	const int safe_delay = std::clamp(settings_snapshot.return_delay, 75, 500);
	const HWND window = gui::get_window();
	if (!cache.is_valid() || !cache.player || !cache.instance || !window ||
		GetForegroundWindow() != window || gui::draw || !gui::runtime_ready.load(std::memory_order_acquire) ||
		now - switched_at >= std::chrono::milliseconds(safe_delay))
	{
		restore(cache);
		return;
	}

	// Respect a manual wheel/number-key change made after the automatic swap.
	try
	{
		maps::PlayerInventory inventory = cache.player.getInventory.call();
		const int selected = inventory ? static_cast<int>(inventory.selectedSlot.get()) : -1;
		if (!inventory || platform::jni_context::clear_exception() || selected != axe_slot)
		{
			old_slot = -1;
			restore(cache);
		}
	}
	catch (...)
	{
		platform::jni_context::clear_exception();
		restore(cache);
	}
}

void modules::shield_breaker::on_disable(::cache& cache)
{
	std::lock_guard lock(state_mutex);
	if (trigger_prepared) return;
	restore(cache);
	last_break_target_id = -1;
	last_break_at = {};
}

void modules::shield_breaker::render_options()
{
    gui::settings::render(settings_, module_settings::shield_breaker::schema());
}
