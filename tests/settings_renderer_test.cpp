#include "../src/ui/settings_renderer.hpp"
#include "../src/modules/combat/legit_aim_config.hpp"
#include "../src/modules/combat/trigger_config.hpp"
#include "../src/modules/visual/settings/chams_settings.hpp"
#include "../src/modules/misc/settings/fast_exp_settings.hpp"
#include "../src/modules/visual/settings/radar_settings.hpp"
#include "../src/modules/misc/settings/pot_refill_settings.hpp"
#include <cassert>
#include <set>
#include <string>

namespace {
    std::string current;
    std::set<std::string> drawn;
    int selected_index = -1;
    float float_low = 0, float_high = 0;
    int int_low = 0, int_high = 0;
}
namespace ImGui {
    void PushID(const char* id) { current = id; }
    void PopID() {}
}
namespace gui {
    bool option_toggle(const char*, bool*) { drawn.insert(current); return false; }
    bool option_slider(const char*, float*, float low, float high, const char*, ImGuiSliderFlags) {
        drawn.insert(current); float_low = low; float_high = high; return false;
    }
    bool option_slider(const char*, int*, int low, int high, const char*, ImGuiSliderFlags) {
        drawn.insert(current); int_low = low; int_high = high; return false;
    }
    bool option_range(const char*, float* value, float low, float high, const char*, ImGuiSliderFlags) {
        drawn.insert(current); float_low = low; float_high = high;
        value[0] = 80; value[1] = 90; return true;
    }
    bool option_range(const char*, int*, int low, int high, const char*, ImGuiSliderFlags) {
        drawn.insert(current); int_low = low; int_high = high; return false;
    }
    bool option_combo(const char*, int* value, const char* const[], int count, int) {
        drawn.insert(current); selected_index = *value;
        assert(count > 0); *value = count - 1; return true;
    }
    bool option_color(const char*, float*, ImGuiColorEditFlags) { drawn.insert(current); return false; }
}
int main()
{
    auto legit = legit_aim_engine::config{};
    gui::settings::render(legit, legit_aim_engine::schema());
    assert(selected_index == 0 && legit.point == legit_aim_engine::aim_point::head);
    legit.point = legit_aim_engine::aim_point::upper_body;
    gui::settings::render(legit, legit_aim_engine::schema());
    assert(selected_index == 1);

    trigger_engine::config trigger;
    gui::settings::render(trigger, trigger_engine::schema().first(1));
    assert(float_low == 50 && float_high == 100);
    assert(trigger.cooldown[0] == .8f && trigger.cooldown[1] == .9f);

    module_settings::chams chams;
    drawn.clear();
    gui::settings::render(chams, module_settings::chams::schema());
    assert(!drawn.contains("visible_color") && drawn.contains("hidden_color") && drawn.contains("glow_strength"));
    chams.theme_color = false; chams.through_walls = false; chams.glow = false;
    drawn.clear();
    gui::settings::render(chams, module_settings::chams::schema());
    assert(drawn.contains("visible_color") && !drawn.contains("hidden_color") && !drawn.contains("glow_strength"));

    module_settings::fast_exp exp;
    drawn.clear();
    gui::settings::render(exp, module_settings::fast_exp::schema());
    assert(!drawn.contains("cps") && int_low == 5 && int_high == 40);
    modules::settings::store<module_settings::fast_exp> store;
    assert(gui::settings::edit(store, module_settings::fast_exp::schema(), [](auto& c) { c.min_cps = 48; }));
    assert(store.read().min_cps == 48);
    assert(gui::settings::edit(store, module_settings::fast_exp::schema(), [](auto& c) { c.min_cps = 999; }));
    assert(store.read().min_cps == 50);
    module_settings::radar radar;
    radar.position_x = 2100;
    gui::settings::render(radar, module_settings::radar::schema().subspan(2, 1));
    assert(float_low == 4 && float_high == 1200 && radar.position_x == 2100);
    assert(modules::settings::normalize(radar, module_settings::radar::schema()) && radar.position_x == 2100);

    module_settings::pot_refill refill;
    drawn.clear();
    gui::settings::render(refill, module_settings::pot_refill::schema());
    assert(drawn.contains("delay_ms") && drawn.contains("random_delay") && drawn.contains("slot_order"));
    assert(!drawn.contains("max_delay_ms") && refill.slot_order == 2);
    refill.random_delay = true;
    refill.delay_ms = 700;
    drawn.clear();
    gui::settings::render(refill, module_settings::pot_refill::schema());
    assert(drawn.contains("max_delay_ms") && int_low == 700 && int_high == 1000);
}
