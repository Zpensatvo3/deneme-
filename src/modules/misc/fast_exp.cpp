#include "fast_exp.hpp"
#include "../../services/game/item_bridge.hpp"
#include "../../ui/settings_renderer.hpp"
#include "../ui_redirect.hpp"

void modules::fast_exp::run(::cache&) {}

void modules::fast_exp::render_overlay()
{
    services::game::items::update(enabled);
}

void modules::fast_exp::render_options()
{
    gui::settings::render(settings_, module_settings::fast_exp::schema());
}
