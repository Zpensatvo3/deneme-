#pragma once

#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/pearl_macro_settings.hpp"
#include "../../platform/adapter/target_version.hpp"

namespace modules
{
class pearl_macro : public module
	{
	public:
		pearl_macro() : module("pearl macro", "World") {}

		void run(::cache& cache) override;
		void render_options() override;
		void on_disable(::cache& cache) override;
        void render_overlay() override;
        void on_render_shutdown() override;
        bool render_when_disabled() const override { return mc::target.build_id == 12111; }
		float get_options_height() override { return 190.0f; }
	private:
        std::atomic_bool native_fallback_{false}, reset_requested_{true};
modules::settings::store<module_settings::pearl_macro> settings_{"pearl_macro"};
		enum class State { IDLE, WAIT_SWITCH, HOLD_USE, WAIT_RETURN };
		State state = State::IDLE;
		int old_slot = -1;
		int pearl_slot = -1;
		bool right_click_down = false;
		std::chrono::steady_clock::time_point next_action{};

		void finish(::cache& cache, bool turn_off);
	};
}
