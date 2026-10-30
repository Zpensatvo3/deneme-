#pragma once

#include "../module_support.hpp"

namespace modules
{
class friend_manager : public module
	{
	public:
		friend_manager() : module("friends manager", "Friends")
		{
			display_options = true;
		}

		void run(::cache& cache) override;
		void on_disable(::cache& cache) override;
		void render_options() override;
		float get_options_height() override { return 430.0f; }
	};
}
