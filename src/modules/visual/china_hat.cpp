#include "china_hat.hpp"
#include "cosmetic_renderer.hpp"
#include "../../ui/settings_renderer.hpp"

void modules::china_hat::render_overlay()
{
    if (enabled) cosmetics::draw_hat(settings_.read());
}
void modules::china_hat::render_options()
{
    gui::settings::render(settings_, module_settings::china_hat::schema());
}
