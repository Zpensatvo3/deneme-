#pragma once

#include <chrono>
#include <atomic>
#include <cstdint>

#include "../../mappings.hpp"
#include "legit_aim_config.hpp"
#include "legit_aim_math.hpp"

namespace legit_aim_engine
{
	struct runtime
	{
		std::atomic<std::uint64_t> reset_generation{1};
		std::uint64_t seen_reset = 0;
		jni::object_wrapper world{nullptr, true}, player{nullptr, true};
		config last_config{};
		bool config_valid = false;
		maps::Entity locked_target{ nullptr, true };
		maps::Entity challenger{ nullptr, true };
		std::chrono::steady_clock::time_point last_update{};
		std::chrono::steady_clock::time_point last_click{};
		std::chrono::steady_clock::time_point next_scan{};
		std::chrono::steady_clock::time_point challenger_since{};
		std::chrono::steady_clock::time_point manual_override_until{};
		legit_aim_math::point filtered_point{};
		legit_aim_math::motion_state motion{};
		maths::angles expected_view{};
		float assist_blend = 0.0f;
		bool filtered_point_valid = false;
		bool expected_view_valid = false;
		bool active = false;
	};

	void run(const config& settings, runtime& state, bool enabled, bool require_left_click = true);
	void request_reset(runtime& state);
	void reset(runtime& state);
	void render_options(config& settings, const char* id_suffix);
}
