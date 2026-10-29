#pragma once

#include "../module_support.hpp"
#include "blatant_aim_engine.hpp"

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
		void visit_config(config_visitor& v) override
		{
			blatant_aim_engine::visit_config(v, settings);
}

	private:
		blatant_aim_engine::config settings{};
		blatant_aim_engine::runtime state{};
	};
}
