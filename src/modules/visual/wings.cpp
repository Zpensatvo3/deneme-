#include "wings.hpp"
#include "cosmetic_renderer.hpp"
#include "../../ui/settings_renderer.hpp"

void modules::wings::render_overlay()
{
    const auto generation=reset_.load(std::memory_order_acquire);
    if (!enabled || generation!=applied_) { motion_.clear(); applied_=generation; }
    if (enabled) cosmetics::draw_wings(settings_.read(),motion_);
}
void modules::wings::render_options()
{
    gui::settings::render(settings_, module_settings::wings::schema());
}
