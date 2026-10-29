#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include "../../cache/cache.hpp"
#include "../../platform/jni/jni_context.hpp"
#include <algorithm>
#include <imgui/imgui.h>

#include "mouse_tweaks.hpp"
#include "../../services/game/inventory_bridge.hpp"
#include "../../ui/settings_renderer.hpp"
#include "../ui_redirect.hpp"
#include "../../gui/gui.hpp"

namespace
{
	struct mouse_tweaks_state
	{
		jobject screen = nullptr;
		int rmb_slot = -1;
		int lmb_slot = -1;
		bool rmb_drag_active = false;
		bool lmb_drag_active = false;
		bool rmb_left_original_slot = false;
		bool last_rmb_down = false;
		bool last_lmb_down = false;
		bool cursor_was_empty = true;
		bool cursor_state_known = false;
	};

	mouse_tweaks_state state{};

	bool key_down(int virtual_key)
	{
		return (GetAsyncKeyState(virtual_key) & 0x8000) != 0;
	}
void reset_state(bool release_screen)
	{
		JNIEnv* env = jni::get_env();
		if (release_screen && state.screen && env)
			env->DeleteGlobalRef(state.screen);

		if (release_screen)
			state.screen = nullptr;

		state.rmb_slot = -1;
		state.lmb_slot = -1;
		state.rmb_drag_active = false;
		state.lmb_drag_active = false;
		state.rmb_left_original_slot = false;
		state.last_rmb_down = false;
		state.last_lmb_down = false;
		state.cursor_was_empty = true;
		state.cursor_state_known = false;
	}

	bool track_screen(jobject screen)
	{
		JNIEnv* env = jni::get_env();
		if (!env || !screen)
			return false;

		if (state.screen && env->IsSameObject(state.screen, screen) == JNI_TRUE)
			return true;

		reset_state(true);
		state.screen = env->NewGlobalRef(screen);
		return state.screen && !platform::jni_context::clear_exception();
	}

	bool get_cursor_stack(
		const maps::ScreenHandler& handler,
		maps::ItemStack& stack,
		bool& empty)
	{
		stack = handler.getCursorStack.call();
		if (!stack || platform::jni_context::clear_exception())
		{
			empty = true;
			return false;
		}

		empty = stack.isEmpty.call() == JNI_TRUE;
		return !platform::jni_context::clear_exception();
	}

	bool slot_has_item(const maps::Slot& slot)
	{
		if (!slot || slot.isEnabled.call() != JNI_TRUE || platform::jni_context::clear_exception())
			return false;

		const bool has_stack = slot.hasStack.call() == JNI_TRUE;
		return !platform::jni_context::clear_exception() && has_stack;
	}

	maps::Slot get_slot_by_id(const maps::ScreenHandler& handler, int slot_id)
	{
		if (!handler || slot_id < 0)
			return {};
		maps::DefaultedList slots = handler.slots.get();
		jni::array<maps::Object> array = slots
			? slots.toArray.call()
			: jni::array<maps::Object>{ nullptr };
		if (!slots || !array || platform::jni_context::clear_exception())
			return {};
		for (const maps::Object& object : array.to_vector())
		{
			maps::Slot slot(object.object_instance, false);
			if (slot && slot.id.get() == slot_id && !platform::jni_context::clear_exception())
				return slot;
			platform::jni_context::clear_exception();
		}
		return {};
	}

	bool can_insert_one(const maps::Slot& slot, const maps::ItemStack& cursor_stack)
	{
		if (!slot || !cursor_stack ||
			slot.isEnabled.call() != JNI_TRUE ||
			slot.canInsert.call(cursor_stack) != JNI_TRUE ||
			platform::jni_context::clear_exception())
		{
			return false;
		}

		maps::ItemStack target_stack = slot.getStack.call();
		if (!target_stack || platform::jni_context::clear_exception())
			return false;

		if (target_stack.isEmpty.call() == JNI_TRUE)
			return !platform::jni_context::clear_exception();

		if (platform::jni_context::clear_exception() ||
			cursor_stack.areItemsAndComponentsEqual.call(cursor_stack, target_stack) != JNI_TRUE ||
			platform::jni_context::clear_exception())
		{
			return false;
		}

		const int slot_limit = slot.getMaxItemCount.call(cursor_stack);
		const int item_limit = cursor_stack.getMaxCount.call();
		const int current_count = target_stack.getCount.call();
		if (platform::jni_context::clear_exception())
			return false;

		return current_count < std::min(slot_limit, item_limit);
	}

	bool stacks_are_compatible(
		const maps::ItemStack& first,
		const maps::ItemStack& second)
	{
		if (!first || !second)
			return false;
		const bool first_empty = first.isEmpty.call() == JNI_TRUE;
		const bool second_empty = second.isEmpty.call() == JNI_TRUE;
		if (platform::jni_context::clear_exception())
			return false;
		if (first_empty || second_empty)
			return true;
		return first.areItemsAndComponentsEqual.call(first, second) == JNI_TRUE &&
			!platform::jni_context::clear_exception();
	}

	bool can_merge_entire_stack(
		const maps::Slot& slot,
		const maps::ItemStack& cursor_stack,
		bool& crafting_output)
	{
		crafting_output = false;
		if (!slot || !cursor_stack || slot.isEnabled.call() != JNI_TRUE ||
			platform::jni_context::clear_exception())
			return false;

		maps::ItemStack target_stack = slot.getStack.call();
		if (!target_stack || platform::jni_context::clear_exception() ||
			target_stack.isEmpty.call() == JNI_TRUE || platform::jni_context::clear_exception() ||
			!stacks_are_compatible(target_stack, cursor_stack))
			return false;

		const int cursor_count = cursor_stack.getCount.call();
		const int target_count = target_stack.getCount.call();
		const int cursor_limit = cursor_stack.getMaxCount.call();
		if (platform::jni_context::clear_exception() || cursor_count <= 0 || target_count <= 0 ||
			cursor_count + target_count > cursor_limit)
			return false;

		// Output slots reject insertion. They need only the first pickup click;
		// regular inventory slots use a second click to put the merged stack back
		// onto the cursor, matching Mouse Tweaks' LMB-with-item behavior.
		crafting_output = slot.canInsert.call(cursor_stack) != JNI_TRUE;
		platform::jni_context::clear_exception();
		return true;
	}

	bool click_slot(
		const maps::ClientPlayerInteractionManager& interaction_manager,
		const maps::ScreenHandler& handler,
		const maps::PlayerEntity& player,
		int slot_id,
		int button,
		const maps::SlotActionType& action_type)
	{
		if (!interaction_manager || !handler || !player || !action_type || slot_id < 0)
			return false;

		interaction_manager.clickSlot.call(
			handler.syncId.get(),
			static_cast<jint>(slot_id),
			static_cast<jint>(button),
			action_type,
			player);

		return !platform::jni_context::clear_exception();
	}
}

modules::mouse_tweaks::mouse_tweaks() : module("mouse tweaks", "Player") {}
modules::mouse_tweaks::~mouse_tweaks() = default;

void modules::mouse_tweaks::render_overlay()
{
    if constexpr (mc::target.build_id != 12111) return;
    if (reset_requested_.exchange(false, std::memory_order_acq_rel)) on_render_shutdown();
    const auto settings = settings_.read();
    services::game::inventory_bridge::tick_mouse(enabled, settings.right_click_tweak,
        settings.left_click_with_item, settings.left_click_without_item);
}

void modules::mouse_tweaks::on_render_shutdown()
{
    services::game::inventory_bridge::cancel(services::game::inventory_bridge::feature::mouse);
}

void modules::mouse_tweaks::run(::cache& cache)
{
	if constexpr (mc::target.build_id == 12111) return;
	const auto settings_snapshot = settings_.read();
	if (gui::draw || !cache.is_valid() || !cache.player || !cache.instance || !jni::get_env())
	{
		reset_state(true);
		return;
	}

	try
	{
		jni::frame frame{ 64 };

		maps::Screen current_screen = cache.instance.currentScreen.get();
		if (!current_screen || platform::jni_context::clear_exception() ||
			!current_screen.is_instance_of<maps::HandledScreen>())
		{
			platform::jni_context::clear_exception();
			reset_state(true);
			return;
		}

		if (!track_screen(current_screen.object_instance))
		{
			reset_state(true);
			return;
		}

		maps::HandledScreen screen(current_screen.object_instance, false);
		maps::ScreenHandler handler = screen.handler.get();
		maps::Slot focused_slot = screen.focusedSlot.get();
		if (!handler || platform::jni_context::clear_exception())
		{
			reset_state(false);
			return;
		}

		int focused_slot_id = -1;
		if (focused_slot)
		{
			focused_slot_id = focused_slot.id.get();
			if (platform::jni_context::clear_exception())
			{
				reset_state(false);
				return;
			}
		}

		maps::ItemStack cursor_stack;
		bool cursor_empty = true;
		if (!get_cursor_stack(handler, cursor_stack, cursor_empty))
		{
			reset_state(false);
			return;
		}

		const bool rmb_down = key_down(VK_RBUTTON);
		const bool lmb_down = key_down(VK_LBUTTON);
		const bool shift_down = key_down(VK_LSHIFT) || key_down(VK_RSHIFT);
		if (!state.cursor_state_known)
		{
			state.cursor_was_empty = cursor_empty;
			state.cursor_state_known = true;
		}
		const bool rmb_pressed = rmb_down && !state.last_rmb_down;
		const bool lmb_pressed = lmb_down && !state.last_lmb_down;

		// RMB tweak only starts if an item was already held before pressing RMB.
		// This preserves vanilla's half-stack pickup when RMB starts on an item.
		if (!settings_snapshot.right_click_tweak || !rmb_down)
		{
			state.rmb_drag_active = false;
			state.rmb_slot = -1;
			state.rmb_left_original_slot = false;
		}
		else if (rmb_pressed && !state.cursor_was_empty)
		{
			state.rmb_drag_active = true;
			state.rmb_slot = focused_slot_id;
			state.rmb_left_original_slot = false;
		}

		// LMB tweak starts only when the cursor was empty before the initial click.
		// Once vanilla picks that stack up, dragging can collect compatible stacks.
		if ((!settings_snapshot.left_click_with_item && !settings_snapshot.left_click_without_item) || !lmb_down)
		{
			state.lmb_drag_active = false;
			state.lmb_slot = -1;
		}
		else if (lmb_pressed && state.cursor_was_empty)
		{
			state.lmb_drag_active = true;
			state.lmb_slot = focused_slot_id;
		}

		maps::ClientPlayerInteractionManager interaction_manager =
			cache.instance.interactionManager.get();
		maps::PlayerEntity player = cache.player.toPlayerEntity();
		if (!interaction_manager || !player || platform::jni_context::clear_exception())
		{
			reset_state(false);
			return;
		}

		maps::SlotActionType action_types{};

		// Slot transitions are processed immediately. There is no fixed timer: a
		// fast cursor cannot lose its current slot just because an 18 ms gate is
		// still active, and revisiting a slot places another item as in the mod.
		if (state.rmb_drag_active && focused_slot_id != state.rmb_slot)
		{
			if (!state.rmb_left_original_slot)
			{
				state.rmb_left_original_slot = true;
				if (state.rmb_slot >= 0 && !cursor_empty)
				{
					maps::Slot original_slot = get_slot_by_id(handler, state.rmb_slot);
					if (original_slot && can_insert_one(original_slot, cursor_stack))
					{
						maps::SlotActionType pickup = action_types.PICKUP.get();
						click_slot(interaction_manager, handler, player,
							state.rmb_slot, 1, pickup);
					}
				}
			}

			state.rmb_slot = focused_slot_id;
			if (focused_slot && can_insert_one(focused_slot, cursor_stack))
			{
				maps::SlotActionType pickup = action_types.PICKUP.get();
				click_slot(interaction_manager, handler, player,
					focused_slot_id, 1, pickup);
			}
		}

		if (state.lmb_drag_active && focused_slot_id != state.lmb_slot)
		{
			state.lmb_slot = focused_slot_id;
			if (focused_slot && slot_has_item(focused_slot))
			{
				if (cursor_empty)
				{
					if (shift_down && settings_snapshot.left_click_without_item)
					{
						maps::SlotActionType quick_move = action_types.QUICK_MOVE.get();
						click_slot(interaction_manager, handler, player,
							focused_slot_id, 0, quick_move);
					}
				}
				else if (settings_snapshot.left_click_with_item)
				{
					maps::ItemStack target_stack = focused_slot.getStack.call();
					if (target_stack && !platform::jni_context::clear_exception() &&
						stacks_are_compatible(target_stack, cursor_stack))
					{
						if (shift_down)
						{
							maps::SlotActionType quick_move = action_types.QUICK_MOVE.get();
							click_slot(interaction_manager, handler, player,
								focused_slot_id, 0, quick_move);
						}
						else
						{
							bool crafting_output = false;
							if (can_merge_entire_stack(
								focused_slot, cursor_stack, crafting_output))
							{
								maps::SlotActionType pickup = action_types.PICKUP.get();
								if (click_slot(interaction_manager, handler, player,
									focused_slot_id, 0, pickup) && !crafting_output)
									click_slot(interaction_manager, handler, player,
										focused_slot_id, 0, pickup);
							}
						}
					}
				}
			}
		}

		state.last_rmb_down = rmb_down;
		state.last_lmb_down = lmb_down;
		state.cursor_was_empty = cursor_empty;
	}
	catch (...)
	{
		platform::jni_context::clear_exception();
		reset_state(false);
	}
}

void modules::mouse_tweaks::on_disable(::cache& cache)
{
    if constexpr (mc::target.build_id == 12111) {
        reset_requested_.store(true, std::memory_order_release);
        return;
    }
	(void)cache;
	reset_state(true);
}

void modules::mouse_tweaks::render_options()
{
	gui::settings::render(settings_, module_settings::mouse_tweaks::schema());
	ImGui::TextDisabled("%s", "RMB revisits a slot; LMB collects matching stacks.");
	ImGui::TextDisabled("%s", "Hold Shift while dragging to quick-move hovered stacks.");
}
