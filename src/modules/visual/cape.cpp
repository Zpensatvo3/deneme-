#include "cape.hpp"
#include "../../ui/settings_renderer.hpp"

void modules::cape::render_overlay()
{
    const auto generation=reset_generation_.load(std::memory_order_acquire);
    if (!enabled || generation!=applied_generation_)
    {
        motion_.clear();
        applied_generation_=generation;
    }
    if (enabled) cosmetics::draw_cape(settings_.read(),motion_);
}

void modules::cape::render_options()
{
    gui::settings::render(settings_,module_settings::cape::schema());
}
