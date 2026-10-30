#pragma once

#include "../module_support.hpp"
#include "legit_aim_engine.hpp"
#include "../core/settings_store.hpp"

namespace modules
{
class legit_click_aim_assist : public module
	{
	public:
		legit_click_aim_assist() : module("legit click aim assist") {}

		void run(::cache& cache) override;
		void render_overlay() override;
		bool render_when_disabled() const override { return true; }
		void on_enable(::cache& cache) override;
		void on_disable(::cache& cache) override;
		void render_options() override;
		float get_options_height() override { return 280.0f; }
	private:
		settings::store<legit_aim_engine::config> settings{"legit_click_aim_assist", legit_aim_engine::schema()};
		legit_aim_engine::runtime state{};
	};
}
