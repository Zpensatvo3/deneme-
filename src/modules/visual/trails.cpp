#include "trails.hpp"
#include "../../ui/settings_renderer.hpp"

void modules::trails::render_overlay()
{
    const auto generation=reset_generation_.load(std::memory_order_acquire);
    if (!enabled || generation!=applied_generation_)
    {
        state_.clear(); applied_generation_=generation;
    }
    if (enabled) cosmetics::draw_trails(settings_.read(),state_);
}

void modules::trails::render_options()
{
    gui::settings::render(settings_,module_settings::trails::schema());
}
