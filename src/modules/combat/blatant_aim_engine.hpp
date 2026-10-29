#pragma once

#include <chrono>
#include <string>

#include "../../cache/cache.hpp"
#include "blatant_aim_config.hpp"

namespace blatant_aim_engine
{
	struct runtime
	{
		bool active = false;
		bool left_button_was_down = false;
		int locked_target_id = -1;
		std::string locked_target_name{};
		int random_target_id = -1;
		float random_ratio = 0.58f;
		float random_goal_ratio = 0.58f;
		std::chrono::steady_clock::time_point random_next_shift{};
		std::chrono::steady_clock::time_point last_update{};
	};

	void run(::cache& cache, config& settings, runtime& state);
	void reset(runtime& state);
	void render_options(config& settings, const char* id_suffix);
}
