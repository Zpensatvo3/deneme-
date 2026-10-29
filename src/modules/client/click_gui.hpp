#pragma once

#include "../module_support.hpp"

namespace modules
{
class click_gui : public module
	{
	public:
		click_gui() : module("click gui", "Client") {}
		bool supports_keybind() const override { return false; } // Fixed menu shortcut.
	};
}
