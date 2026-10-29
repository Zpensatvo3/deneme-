#pragma once

#include "../module_support.hpp"

namespace modules
{
class panic : public module
	{
	public:
		panic() : module("Self Destruct", "Client", "panic") {}
		void run(::cache& cache) override;
	};
}
