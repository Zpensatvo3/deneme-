#include "jump_circles.hpp"
#include "../../ui/settings_renderer.hpp"
void modules::jump_circles::render_overlay()
{
    const auto generation = reset_.load(std::memory_order_acquire);
    if (!enabled || generation != applied_) { state_.clear(); applied_ = generation; }
    if (enabled) cosmetics::draw_jump_circles(settings_.read(), state_);
}
void modules::jump_circles::render_options() { gui::settings::render(settings_, module_settings::jump_circles::schema()); }
