#pragma once

#include "../module_support.hpp"
#include "../../gui/gui.hpp"

namespace modules
{
class watermark : public module
	{
	public:
		watermark() : module("watermark", "Visual") {}

		void run(::cache& cache) override;
		void render_overlay() override;
		void render_options() override;
		std::pair<std::string, int> status_snapshot() const;
		float get_options_height() override { return 86.0f; }
	private:
		mutable std::mutex status_mutex{};
		std::string player_name{ "Player" };
		int latency_ms = 0;
		std::chrono::steady_clock::time_point next_status_refresh{};
	};
}
