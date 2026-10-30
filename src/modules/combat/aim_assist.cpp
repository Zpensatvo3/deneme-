#include "aim_assist.hpp"

void modules::aim_assist::run(cache&)
{
	// Shared camera engine executes only on the render/client thread.
}

void modules::aim_assist::render_overlay()
{
	legit_aim_engine::run(settings.read(), state, enabled, false);
}

void modules::aim_assist::on_enable(cache&)
{
	legit_aim_engine::request_reset(state);
}

void modules::aim_assist::on_disable(cache&)
{
	legit_aim_engine::request_reset(state);
}

void modules::aim_assist::render_options()
{
	settings.update([&](auto& candidate) { legit_aim_engine::render_options(candidate, "aim_assist_v2"); return true; });
}
