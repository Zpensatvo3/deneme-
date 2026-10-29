#include "hit_particles.hpp"
#include "../../ui/settings_renderer.hpp"
void modules::hit_particles::render_overlay()
{
    const auto generation = reset_.load(std::memory_order_acquire);
    if (!enabled || generation != applied_) { state_.clear(); applied_ = generation; }
    if (enabled) cosmetics::draw_hit_particles(settings_.read(), state_);
}
void modules::hit_particles::render_options() { gui::settings::render(settings_, module_settings::hit_particles::schema()); }
