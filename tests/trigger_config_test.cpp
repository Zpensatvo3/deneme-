// Reuse the existing native-free module fixture, not the gameplay adapter.
#define main existing_config_fields_round_trip_test
#include "config_fields_test.cpp"
#undef main
#include "../src/modules/combat/trigger_config.hpp"
#include <cassert>
#include <limits>

class trigger_fixture final : public modules::config_fixture
{
public:
    trigger_fixture() : config_fixture("triggerbot") {}
    trigger_engine::config settings;
    int commits = 0;
    void visit_config(modules::config_visitor& v) override
    {
        if (trigger_engine::visit_config(v, settings)) ++commits;
    }
};
int main()
{
    using namespace trigger_engine;
    config invalid;
    invalid.cooldown[0] = std::numeric_limits<float>::quiet_NaN();
    assert(!normalize(invalid));
    invalid = {}; invalid.weapon = static_cast<weapon_mode>(9);
    assert(!normalize(invalid));
    invalid = {}; invalid.cooldown[0] = 2.f; invalid.cooldown[1] = .1f;
    invalid.reaction[0] = 200; invalid.reaction[1] = -10;
    assert(normalize(invalid) && invalid.cooldown[0] == .5f && invalid.cooldown[1] == 1.f &&
        invalid.reaction[0] == 0 && invalid.reaction[1] == 150);
    config copy = invalid;
    assert(same_config(invalid, copy));
    copy.click_simulation = !copy.click_simulation;
    assert(!same_config(invalid, copy));
    trigger_fixture fixture;
    const auto initial = fixture.export_config_settings();
    assert(initial.find("prioritize_crits") == std::string::npos && initial.find("min_cooldown") == std::string::npos);
    assert(fixture.validate_config_settings(initial) && fixture.commits == 0);
    assert(fixture.apply_config_settings(initial) && fixture.commits == 1);
    assert(fixture.apply_config_settings(R"({"cooldown":[0.99,0.6],"reaction":[120,10],"targets":2,"weapon":1,"criticals":2,"on_click":true})"));
    assert(fixture.settings.cooldown[0] == .6f && fixture.settings.cooldown[1] == .99f);
    assert(fixture.settings.reaction[0] == 10 && fixture.settings.reaction[1] == 120 && fixture.settings.criticals == critical_mode::only);
    assert(fixture.apply_config_settings(R"({"cooldown":[0.7,0.8],"min_cooldown":"ignored","reaction":[20,40],"reaction_delay_min":99,"targets":0,"players":false,"weapon":2,"sword_only":true,"criticals":2,"wait_critical":false,"prioritize_crits":true,"on_click":false,"on_left_click":true})"));
    assert(fixture.settings.cooldown[0] == .7f && fixture.settings.reaction[0] == 20 && fixture.settings.targets == target_mode::players);
    assert(fixture.settings.weapon == weapon_mode::axe && fixture.settings.criticals == critical_mode::only && !fixture.settings.on_click);
    assert(fixture.apply_config_settings(R"({"criticals":0,"prioritize_crits":true})"));
    assert(fixture.settings.criticals == critical_mode::smart);
    assert(fixture.apply_config_settings(R"({"weapon":1})"));
    assert(fixture.apply_config_settings(R"({"axe_only":true})"));
    assert(fixture.settings.weapon == weapon_mode::axe);
    assert(fixture.apply_config_settings(R"({"wait_critical":true,"sword_only":false,"axe_only":false,"players":true,"mobs":true,"animals":true,"min_cooldown":0.8,"max_cooldown":0.9,"reaction_delay_min":12,"reaction_delay_max":15,"on_left_click":true})"));
    assert(fixture.settings.criticals == critical_mode::only && fixture.settings.weapon == weapon_mode::any);
    assert(fixture.settings.targets == target_mode::all_living && fixture.settings.on_click);
    const auto before = fixture.export_config_settings(); const int commits = fixture.commits;
    for (const auto* bad : {R"({"weapon":3})", R"({"targets":-1})", R"({"criticals":10})",
        R"({"players":false,"mobs":true})", R"({"players":true,"mobs":false,"animals":true})",
        R"({"sword_only":true,"axe_only":true})", R"({"cooldown":[0.9,"bad"]})",
        R"({"reaction":[1,2],"on_click":"bad"})", R"({"min_cooldown":1e999})"}) {
        assert(!fixture.validate_config_settings(bad));
        assert(!fixture.apply_config_settings(bad));
        assert(fixture.export_config_settings() == before && fixture.commits == commits);
    }
    assert(fixture.validation_error(R"({"players":false})").find("cannot be represented safely") != std::string::npos);
    assert(fixture.apply_config_settings("{}") && fixture.commits == commits + 1);
    std::cout << "Trigger config compatibility, precedence, atomic import and diagnostics passed.\n";
}
