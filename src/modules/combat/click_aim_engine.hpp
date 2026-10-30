#pragma once

#include <chrono>

#include "../../cache/cache.hpp"
#include "click_aim_config.hpp"

namespace click_aim_engine
{
	struct runtime
	{
		bool active = false;
		bool left_button_was_down = false;
		jni::object_wrapper world{ nullptr, true }, player{ nullptr, true };
		maps::Entity locked_target{ nullptr, true };
		std::chrono::steady_clock::time_point lock_lost_since{};
		std::chrono::steady_clock::time_point released_since{};
		maps::Entity aim_target{ nullptr, true };
		int selected_region = -1;
		bool aim_complete = false;
		std::chrono::steady_clock::time_point last_update{};
	};

	void run(::cache& cache, config& settings, runtime& state);
	void reset(runtime& state);
	void render_options(config& settings, const char* id_suffix);
}
