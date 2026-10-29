#pragma once

#include "../module_support.hpp"
#include "legit_aim_engine.hpp"

namespace modules
{
class aim_assist : public module
	{
	public:
		aim_assist() : module("aim assist") { settings.weapon_only = false; }

		void run(::cache& cache) override;
		void render_overlay() override;
		bool render_when_disabled() const override { return true; }
		void on_enable(::cache& cache) override;
		void on_disable(::cache& cache) override;
		void render_options() override;
		float get_options_height() override { return 280.0f; }
		void visit_config(config_visitor& v) override
		{
			if (legit_aim_engine::visit_config(v, settings)) legit_aim_engine::request_reset(state);
		}

	private:
		legit_aim_engine::config settings{};
		legit_aim_engine::runtime state{};
	};
}
