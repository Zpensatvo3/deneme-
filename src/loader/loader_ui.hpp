#pragma once

#include <string>

namespace loader_ui
{
	struct state
	{
		int stage = 0;
		bool completed = false;
		bool failed = false;
		bool closing = false;
		std::string detail;
	};

	void initialize();
	void stage(int index, const char* detail);
	void ready();
	void fail(const char* detail);
	void closing();
	state snapshot();
}

