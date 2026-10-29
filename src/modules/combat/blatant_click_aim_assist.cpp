#include "blatant_click_aim_assist.hpp"

void modules::blatant_click_aim_assist::run(cache& cache)
{
	blatant_aim_engine::run(cache, settings, state);
}

void modules::blatant_click_aim_assist::on_disable(cache&)
{
	state = {};
}

void modules::blatant_click_aim_assist::render_options()
{
	blatant_aim_engine::render_options(settings, "blatant_click_aimassist");
}
