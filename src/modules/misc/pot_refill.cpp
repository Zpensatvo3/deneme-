#include <Windows.h>
#include "../../cache/cache.hpp"
#include "../../platform/jni/jni_context.hpp"
#include <algorithm>
#include <chrono>

#include <imgui/imgui.h>

#include "pot_refill.hpp"
#include "../../ui/settings_renderer.hpp"
#include "../ui_redirect.hpp"
#include "../../gui/gui.hpp"
#include "../../mappings.hpp"
#include "pot_refill_engine.hpp"
#include "../../services/game/overlay_context.hpp"
#include "../../services/game/inventory_bridge.hpp"
#include "../../services/input/input_service.hpp"
#include <vector>


namespace
{
	using steady_clock = std::chrono::steady_clock;
bool is_instant_health_potion(const maps::ItemStack& stack)
	{
		if (!stack || stack.isEmpty.call() == JNI_TRUE || platform::jni_context::clear_exception())
			return false;

		maps::Item item = stack.item.get();
		if (!item || platform::jni_context::clear_exception() ||
			!item.is_instance_of<maps::PotionItem>() || platform::jni_context::clear_exception())
			return false;

		maps::ComponentType potion_type = maps::DataComponentTypes{}.POTION_CONTENTS.get();
		if (!potion_type || platform::jni_context::clear_exception())
			return false;

		maps::Object component = stack.getComponent.call(potion_type);
		if (!component || platform::jni_context::clear_exception())
			return false;

		maps::PotionContentsComponent contents(component.object_instance, false);
		maps::RegistryEntry healing = maps::Potions{}.HEALING.get();
		maps::RegistryEntry strong_healing = maps::Potions{}.STRONG_HEALING.get();
		if (!contents || !healing || !strong_healing || platform::jni_context::clear_exception())
			return false;

		const bool healing_one = contents.matches.call(healing) == JNI_TRUE;
		if (platform::jni_context::clear_exception())
			return false;
		const bool healing_two = contents.matches.call(strong_healing) == JNI_TRUE;
		return !platform::jni_context::clear_exception() && (healing_one || healing_two);
	}

	struct observed_slot
	{
		modules::pot_refill_logic::slot data;
		maps::Slot slot;
		maps::ItemStack stack;
	};

	bool find_player_slots(const maps::ScreenHandler& handler,
		const maps::PlayerInventory& player_inventory,
		std::vector<observed_slot>& output,
		int& empty_count,
		int& potion_count)
	{
		output.clear();
		empty_count = 0;
		potion_count = 0;

		maps::DefaultedList slots = handler ? handler.slots.get() : maps::DefaultedList{};
		jni::array<maps::Object> array = slots
			? slots.toArray.call()
			: jni::array<maps::Object>{ nullptr };
		if (!slots || !array || !player_inventory || platform::jni_context::clear_exception())
			return false;

		maps::Inventory player_inventory_view(player_inventory.object_instance, false);
		for (const maps::Object& object : array.to_vector())
		{
			maps::Slot slot(object.object_instance, false);
			maps::Inventory slot_inventory = slot ? slot.inventory.get() : maps::Inventory{};
			if (!slot || !slot_inventory || platform::jni_context::clear_exception() ||
				!slot_inventory.is_same_object(player_inventory_view))
				continue;

			const int inventory_index = slot.index.get();
			const int screen_slot = slot.id.get();
			if (inventory_index < 0 || inventory_index >= 36 || screen_slot < 0 ||
				slot.isEnabled.call() != JNI_TRUE || platform::jni_context::clear_exception())
				continue;
			maps::ItemStack stack = slot.getStack.call();
			if (!stack || platform::jni_context::clear_exception())
				continue;

			const bool empty = stack.isEmpty.call() == JNI_TRUE;
			if (platform::jni_context::clear_exception())
				continue;

			const bool healing = !empty && is_instant_health_potion(stack);
			const int count = empty ? 0 : stack.getCount.call();
			if (platform::jni_context::clear_exception()) return false;
			if (inventory_index < 9 && empty) ++empty_count;
			if (inventory_index >= 9 && healing) ++potion_count;
			output.push_back({{inventory_index, screen_slot, empty, healing, count}, slot, stack});
		}

		return true;
	}
}

struct modules::pot_refill::runtime
{
	pot_refill_logic::engine engine;
	services::game::overlay_session session;
	maps::Screen screen{nullptr, true};
	maps::ScreenHandler handler{nullptr, true};
	maps::ItemStack expected{nullptr, true};
	int sync_id = -1;
	std::optional<module_settings::pot_refill> settings;
	void reset()
	{
		engine.reset();
		session.reset();
		screen = maps::Screen{};
		handler = maps::ScreenHandler{};
		expected = maps::ItemStack{};
		sync_id = -1;
	}
};

modules::pot_refill::pot_refill() : module("pot refill", "Player"), runtime_(std::make_unique<runtime>()) {}
modules::pot_refill::~pot_refill() = default;

void modules::pot_refill::run(::cache&)
{
	// Inventory packets are issued from render_overlay(), which runs on the
	// Minecraft client/render thread. The DLL worker thread stays read-only.
}

void modules::pot_refill::render_overlay()
{
	const auto settings_snapshot = settings_.read();
	if (!platform::jni_context::bind_current_thread())
	{
		reset_requested.store(true, std::memory_order_release);
		return;
	}

	jni::frame frame{ 512 };
	auto& state = *runtime_;
	const auto reset = [&] {
		state.reset();
        services::game::inventory_bridge::cancel(services::game::inventory_bridge::feature::refill);
		live_status = live_health_potions = live_empty_hotbar = 0;
	};
	if (reset_requested.exchange(false, std::memory_order_acq_rel) ||
		!state.settings || *state.settings != settings_snapshot)
	{
		reset();
		state.settings = settings_snapshot;
	}

	const HWND game_window = gui::get_window();
	if (!enabled || !game_window || GetForegroundWindow() != game_window || gui::loader_visible)
	{
		reset();
		return;
	}
	if (gui::draw)
	{
		// Keep explicitly labelled last-scan diagnostics visible in the menu,
		// but discard every pending game action while the menu has focus.
		state.reset();
        services::game::inventory_bridge::cancel(services::game::inventory_bridge::feature::refill);
		live_status = 9;
		return;
	}

    namespace bridge = services::game::inventory_bridge;
    bridge::container_snapshot observed;
    const auto backend = bridge::capture_container(bridge::feature::refill, observed);
    if (backend != services::game::hybrid::result::native_fallback) {
        if (backend != services::game::hybrid::result::ready) { reset(); return; }
        const steady_clock::time_point now = steady_clock::now();
        maps::ClientWorld world(observed.world, false);
        maps::ClientPlayerEntity player(observed.player, false);
        maps::Screen screen(observed.screen, false);
        maps::ScreenHandler handler(observed.handler, false);
        const bool same_session = state.session.matches({world, player}) && state.screen.is_same_object(screen) &&
            state.handler.is_same_object(handler) && state.sync_id == observed.sync_id;
        if (!same_session || platform::jni_context::clear_exception()) {
            reset(); state.session.observe({world, player}); state.screen = screen; state.handler = handler;
            state.sync_id = observed.sync_id;
            state.engine.arm(now, pot_refill_logic::delay(settings_snapshot, gen));
        }
        if (!observed.cursor_empty || services::input::physical_button_down(services::input::mouse_button::left) ||
            services::input::physical_button_down(services::input::mouse_button::right)) {
            reset(); live_status = 2; return;
        }
        live_empty_hotbar = live_health_potions = 0;
        std::vector<pot_refill_logic::slot> slots;
        for (const auto& slot : observed.slots) {
            if (!slot.enabled || slot.index < 0 || slot.index >= 36) continue;
            if (slot.index < 9 && slot.empty) ++live_empty_hotbar;
            if (slot.index >= 9 && slot.healing) ++live_health_potions;
            slots.push_back({slot.index, slot.id, slot.empty, slot.healing, slot.count});
        }
        if (state.engine.pending()) {
            const auto result = state.engine.observe(now, observed.confirmed);
            if (result == pot_refill_logic::observation::completed) {
                bridge::cancel(bridge::feature::refill);
                state.engine.arm(now, pot_refill_logic::delay(settings_snapshot, gen)); live_status = 6;
            } else if (result == pot_refill_logic::observation::timed_out) {
                bridge::cancel(bridge::feature::refill); live_status = 7;
            } else live_status = 8;
            return;
        }
        if (state.engine.blocked()) { live_status = 7; return; }
        if (!live_empty_hotbar) { live_status = 3; return; }
        if (!live_health_potions) { live_status = 4; return; }
        if (!state.engine.ready(now)) return;
        const auto action = pot_refill_logic::select(slots, settings_snapshot.slot_order, gen);
        if (!action) { live_status = 1; return; }
        if (!state.engine.submitted(*action, now)) return;
        if (!bridge::container_act(observed, 0, action->source_screen_id, action->target_screen_id)) {
            state.engine.stop(); bridge::cancel(bridge::feature::refill); live_status = 5;
        } else live_status = 8;
        return;
    }

	services::game::client_context context;
	if (!services::game::capture_client_context(context) || !context.screen ||
		!context.screen.is_instance_of<maps::HandledScreen>() || platform::jni_context::clear_exception() ||
		context.screen.is_instance_of<maps::CreativeInventoryScreen>() || platform::jni_context::clear_exception())
	{
		reset();
		return;
	}
	auto& player = context.player;

	const steady_clock::time_point now = steady_clock::now();
	maps::HandledScreen inventory_screen(context.screen.object_instance, false);
	maps::ScreenHandler handler = inventory_screen.handler.get();
	maps::ScreenHandler player_handler = player.currentScreenHandler.get();
	if (!handler || !player_handler || platform::jni_context::clear_exception() ||
		!handler.is_same_object(player_handler))
	{
		reset();
		live_status = 1;
		return;
	}
	const int sync_id = handler.syncId.get();
	if (platform::jni_context::clear_exception()) { reset(); live_status = 1; return; }
	const bool same_session = state.session.matches({context.world, player}) &&
		state.screen.is_same_object(context.screen) && state.handler.is_same_object(handler) && state.sync_id == sync_id;
	if (!same_session || platform::jni_context::clear_exception())
	{
		reset();
		state.session.observe({context.world, player});
		state.screen = context.screen;
		state.handler = handler;
		state.sync_id = sync_id;
		state.engine.arm(now, pot_refill_logic::delay(settings_snapshot, gen));
	}

	// Never disturb an item the user is already holding with the cursor.
	maps::ItemStack cursor_stack = handler.getCursorStack.call();
	if (!cursor_stack || platform::jni_context::clear_exception() ||
		cursor_stack.isEmpty.call() != JNI_TRUE || platform::jni_context::clear_exception() ||
		services::input::physical_button_down(services::input::mouse_button::left) ||
		services::input::physical_button_down(services::input::mouse_button::right))
	{
		reset();
		live_status = 2;
		return;
	}

	maps::PlayerInventory inventory = player.getInventory.call();
	if (!inventory || platform::jni_context::clear_exception())
	{
		reset();
		live_status = 1;
		return;
	}

	std::vector<observed_slot> slots;
	if (!find_player_slots(handler, inventory, slots,
		live_empty_hotbar, live_health_potions))
	{
		reset();
		live_status = 1;
		return;
	}
	const auto find = [&](int index, int id) -> const observed_slot* {
		const observed_slot* result = nullptr;
		for (const auto& slot : slots)
			if (slot.data.inventory_index == index && slot.data.screen_id == id)
			{
				if (result) return nullptr;
				result = &slot;
			}
		return result;
	};
	if (state.engine.pending())
	{
		const auto action = *state.engine.pending();
		const auto* source = find(action.source_index, action.source_screen_id);
		const auto* target = find(action.target_index, action.target_screen_id);
		bool observed = source && target && source->data.empty && !target->data.empty && state.expected;
		if (observed)
		{
			observed = maps::ItemStack{}.areItemsAndComponentsEqual.call(target->stack, state.expected) == JNI_TRUE &&
				target->data.count == state.expected.getCount.call();
			if (platform::jni_context::clear_exception()) observed = false;
		}
		// This validates fresh client inventory state, not a server ACK. Vanilla
		// predicts clicks locally; every subsequent action still rescans all slots.
		const auto result = state.engine.observe(now, observed);
		if (result == pot_refill_logic::observation::completed)
		{
			state.expected = maps::ItemStack{};
			state.engine.arm(now, pot_refill_logic::delay(settings_snapshot, gen));
			live_status = 6;
		}
		else if (result == pot_refill_logic::observation::timed_out)
		{
			state.expected = maps::ItemStack{};
			live_status = 7;
		}
		else live_status = 8;
		return;
	}
	if (state.engine.blocked()) { live_status = 7; return; }
	if (live_empty_hotbar == 0)
	{
		live_status = 3;
		return;
	}
	if (live_health_potions == 0)
	{
		live_status = 4;
		return;
	}

	if (!state.engine.ready(now)) return;
	std::vector<pot_refill_logic::slot> candidates;
	for (const auto& slot : slots) candidates.push_back(slot.data);
	const auto action = pot_refill_logic::select(candidates, settings_snapshot.slot_order, gen);
	if (!action) { live_status = 1; return; }
	const auto* source = find(action->source_index, action->source_screen_id);
	const auto* target = find(action->target_index, action->target_screen_id);
	if (!source || !target) { live_status = 1; return; }
	// Re-read immediately before SWAP. Never replace any occupied hotbar slot.
	auto source_stack = source->slot.getStack.call();
	auto target_stack = target->slot.getStack.call();
	if (!source_stack || !target_stack || platform::jni_context::clear_exception() ||
		target_stack.isEmpty.call() != JNI_TRUE || !is_instant_health_potion(source_stack) ||
		target->slot.canInsert.call(source_stack) != JNI_TRUE || platform::jni_context::clear_exception())
	{ live_status = 2; return; }
	state.expected = source_stack.copy.call();
	if (!state.expected || platform::jni_context::clear_exception()) { live_status = 5; return; }

	maps::SlotActionType swap_action = maps::SlotActionType{}.SWAP.get();
	if (!swap_action || platform::jni_context::clear_exception())
	{
		live_status = 5;
		return;
	}

	// SWAP's button is the destination hotbar inventory index (0..8). The source
	// ID comes from the active handler's real Slot object, so custom layouts and
	// remapped clients do not depend on PlayerScreenHandler's visual slot order.
	if (!state.engine.submitted(*action, now)) return;
	context.interaction.clickSlot.call(
		sync_id,
		static_cast<jint>(action->source_screen_id),
		static_cast<jint>(action->target_index),
		swap_action,
		player.toPlayerEntity());

	if (!platform::jni_context::clear_exception())
	{
		live_status = 8;
	}
	else
	{
		state.engine.stop();
		state.expected = maps::ItemStack{};
		live_status = 5;
	}
}

void modules::pot_refill::on_disable(::cache&)
{
	reset_requested.store(true, std::memory_order_release);
}

void modules::pot_refill::on_render_shutdown()
{
    runtime_->reset();
    services::game::inventory_bridge::cancel(services::game::inventory_bridge::feature::refill);
}

void modules::pot_refill::render_options()
{
	gui::settings::render(settings_, module_settings::pot_refill::schema());
	ImGui::TextDisabled("%s", "Last inventory scan:");
	ImGui::TextDisabled("%s: %d | %s: %d", "Health Potions", live_health_potions,
		"Empty Hotbar", live_empty_hotbar);
	const char* status = "Open inventory to refill";
	switch (live_status)
	{
	case 1: status = "Waiting for inventory"; break;
	case 2: status = "Paused for manual interaction"; break;
	case 3: status = "Hotbar is full"; break;
	case 4: status = "No health potions in inventory"; break;
	case 5: case 7: status = "Stopped: reopen inventory"; break;
	case 6: status = "Refilling"; break;
	case 8: status = "Checking inventory change"; break;
	case 9: status = "Paused while menu is open"; break;
	}
	ImGui::TextDisabled("%s", status);
}
