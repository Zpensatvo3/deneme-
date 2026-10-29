#include "loader_ui.hpp"

#include <algorithm>
#include <mutex>

namespace
{
	std::mutex state_mutex;
	loader_ui::state current_state{};
}

void loader_ui::initialize()
{
	std::lock_guard lock(state_mutex);
	current_state = {};
	current_state.detail = "Preparing native runtime";
}

void loader_ui::stage(int index, const char* detail)
{
	std::lock_guard lock(state_mutex);
	if (current_state.failed)
		return;
	current_state.stage = std::clamp(index, 0, 4);
	current_state.detail = detail ? detail : "Working";
}

void loader_ui::ready()
{
	std::lock_guard lock(state_mutex);
	if (current_state.failed)
		return;
	current_state.stage = 4;
	current_state.completed = true;
	current_state.detail = "onestep is ready";
}

void loader_ui::fail(const char* detail)
{
	std::lock_guard lock(state_mutex);
	current_state.failed = true;
	current_state.detail = detail ? detail : "Unknown initialization error";
}

void loader_ui::closing()
{
	std::lock_guard lock(state_mutex);
	current_state.closing = true;
}

loader_ui::state loader_ui::snapshot()
{
	std::lock_guard lock(state_mutex);
	return current_state;
}
