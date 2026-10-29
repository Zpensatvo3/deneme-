#pragma once

#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/shield_breaker_settings.hpp"
#include "../../services/game/client_context.hpp"

namespace modules
{
class shield_breaker : public module
	{
	public:
		shield_breaker() : module("shield breaker", "Combat") { active_instance.store(this); }
		~shield_breaker() override
		{
			shield_breaker* expected = this;
			active_instance.compare_exchange_strong(expected, nullptr);
		}

		void run(::cache& cache) override;
		void render_overlay() override;
		void render_options() override;
		void on_disable(::cache& cache) override;
		float get_options_height() override { return 150.0f; }
		void visit_config(config_visitor& v) override
		{ settings_.visit(v, module_settings::shield_breaker::schema()); }

		// Runs on Minecraft's window thread before the physical click reaches
		// GLFW, so the vanilla attack itself is performed with the axe selected.
		static bool gate_left_click();
		static bool prepare_for_trigger_attack();
		static int trigger_prepared_slot();
		static int starting_slot_if_owned(const maps::ClientPlayerEntity& player,
			const maps::ClientWorld& world, int selected);
		static void finish_trigger_preparation(const maps::ClientPlayerEntity& player,
			const maps::ClientPlayerInteractionManager& manager, bool consumed);

	private:
        modules::settings::store<module_settings::shield_breaker> settings_;
		inline static std::atomic<shield_breaker*> active_instance{ nullptr };
		int old_slot = -1;
		int axe_slot = -1;
		bool waiting_to_restore = false;
		bool trigger_prepared = false;
		jni::object_wrapper trigger_world{nullptr, true}, trigger_player{nullptr, true};
		std::chrono::steady_clock::time_point switched_at{};
		std::chrono::steady_clock::time_point last_break_at{};
		int last_break_target_id = -1;
		std::mutex state_mutex{};

		bool prepare_current_thread();
		bool break_current_target();
		void restore(::cache& cache);
	};
}
