#include "blatant_click_aim_assist.hpp"

void modules::blatant_click_aim_assist::run(cache& cache)
{
	auto config = settings.read();
	blatant_aim_engine::run(cache, config, state);
}

void modules::blatant_click_aim_assist::on_disable(cache&)
{
	state = {};
}

void modules::blatant_click_aim_assist::render_options()
{
	settings.update([&](auto& candidate) { blatant_aim_engine::render_options(candidate, "blatant_click_aimassist"); return true; });
}
