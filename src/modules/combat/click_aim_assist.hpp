#pragma once

#include "../module_support.hpp"
#include "click_aim_engine.hpp"
#include "../core/settings_store.hpp"

namespace modules
{
class click_aim_assist : public module
	{
	public:
		click_aim_assist() : module("click aim assist") {}

		void run(::cache& cache) override;
		void on_enable(::cache& cache) override;
		void on_disable(::cache& cache) override;
		void render_options() override;
		float get_options_height() override { return 650.0f; }
	private:
		settings::store<click_aim_engine::config> settings{"click_aim_assist", click_aim_engine::schema()};
		click_aim_engine::runtime state{};
	};
}
