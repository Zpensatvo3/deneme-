#include "click_aim_assist.hpp"

void modules::click_aim_assist::run(cache& cache)
{
	auto config = settings.read();
	click_aim_engine::run(cache, config, state);
}

void modules::click_aim_assist::on_enable(cache&)
{
	click_aim_engine::reset(state);
}

void modules::click_aim_assist::on_disable(cache&)
{
	click_aim_engine::reset(state);
}

void modules::click_aim_assist::render_options()
{
	settings.update([&](auto& candidate) { click_aim_engine::render_options(candidate, "click_aimassist"); return true; });
}
