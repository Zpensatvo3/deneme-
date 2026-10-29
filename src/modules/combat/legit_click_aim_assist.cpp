#include "legit_click_aim_assist.hpp"

void modules::legit_click_aim_assist::run(cache&)
{
	// Camera state is no longer read/written from the background module worker.
}

void modules::legit_click_aim_assist::render_overlay()
{
	legit_aim_engine::run(settings, state, enabled, true);
}

void modules::legit_click_aim_assist::on_enable(cache&)
{
	legit_aim_engine::request_reset(state);
}

void modules::legit_click_aim_assist::on_disable(cache&)
{
	legit_aim_engine::request_reset(state);
}

void modules::legit_click_aim_assist::render_options()
{
	legit_aim_engine::render_options(settings, "legit_click_aimassist_v2");
}
