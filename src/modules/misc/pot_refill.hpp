#pragma once

#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/pot_refill_settings.hpp"
#include <memory>

namespace modules
{
class pot_refill : public module
	{
	public:
		pot_refill();
		~pot_refill() override;

		void run(::cache& cache) override;
		void render_overlay() override;
		void render_options() override;
		void on_disable(::cache& cache) override;
		void on_render_shutdown() override;
		bool render_when_disabled() const override { return true; }
		float get_options_height() override { return 240.0f; }
	private:
modules::settings::store<module_settings::pot_refill> settings_{"pot_refill"};
		struct runtime;
		std::unique_ptr<runtime> runtime_;
		std::atomic_bool reset_requested{ true };
		int live_health_potions = 0;
		int live_empty_hotbar = 0;
	};
}
