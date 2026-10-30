#pragma once

#include "../module_support.hpp"
#include "blatant_aim_engine.hpp"
#include "../core/settings_store.hpp"

namespace modules
{
class blatant_click_aim_assist : public module
	{
	public:
		blatant_click_aim_assist() : module("blatant click aim assist") {}

		void run(::cache& cache) override;
		void on_disable(::cache& cache) override;
		void render_options() override;
		float get_options_height() override { return 370.0f; }
	private:
		settings::store<blatant_aim_engine::config> settings{"blatant_click_aim_assist", blatant_aim_engine::schema()};
		blatant_aim_engine::runtime state{};
	};
}
