#pragma once

#include "../module_support.hpp"
#include "../../platform/adapter/target_version.hpp"

namespace modules
{
class full_bright : public module
	{
	public:
		full_bright() : module("full bright", "Visual") {}

		void run(::cache& cache) override;
		void on_disable(::cache& cache) override;
        void render_overlay() override;
        void on_render_shutdown() override;
        bool render_when_disabled() const override { return mc::target.build_id == 12111; }

	private:
		double original_gamma = 0.5;
		bool original_gamma_captured = false;
        std::atomic_bool native_fallback_{false};
        std::atomic_bool reset_requested_{false};
	};
}
