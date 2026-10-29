#define main existing_config_fields_round_trip_test
#include "config_fields_test.cpp"
#undef main
#include "../src/modules/combat/jump_reset_config.hpp"
#include <cassert>
#include <initializer_list>

class jump_fixture final : public modules::module
{
public:
    jump_fixture() : module("jump_reset") {}
    jump_reset_config::config settings;
    int revisions = 0;
    void visit_config(modules::config_visitor& v) override
    {
        if (jump_reset_config::visit_config(v, settings)) ++revisions;
    }
};

int main()
{
    using namespace jump_reset_config;
    jump_fixture fixture;
    const auto initial = fixture.export_config_settings();
    assert(initial.find("profile") == std::string::npos);
    assert(fixture.validate_config_settings(initial) && fixture.revisions == 0);
    assert(fixture.apply_config_settings(initial) && fixture.revisions == 1);
    assert(fixture.settings.mode == 0 && fixture.settings.chance == 100 && fixture.settings.jump_delay_ms == 0);
    assert(selected_profile(fixture.settings) == profile::immediate);
    assert(fixture.apply_config_settings(R"({"mode":1,"chance":75,"jump_delay_ms":25,"only_combat":false})"));
    assert(fixture.settings.mode == 1 && fixture.settings.chance == 75 && fixture.settings.jump_delay_ms == 25 && !fixture.settings.only_combat);
    assert(selected_profile(fixture.settings) == profile::custom);
    const auto saved = fixture.export_config_settings();
    const int revisions = fixture.revisions;
    for (const auto* invalid : {R"({"mode":-1})", R"({"mode":2})", R"({"mode":"bad"})",
        R"({"chance":1e999})", R"({"chance":20,"only_combat":"bad"})", R"({"jump_delay_ms":"bad"})"}) {
        assert(!fixture.validate_config_settings(invalid));
        assert(!fixture.apply_config_settings(invalid));
        assert(fixture.export_config_settings() == saved && fixture.revisions == revisions);
    }
    assert(fixture.apply_config_settings(R"({"chance":-5,"jump_delay_ms":150})"));
    assert(fixture.settings.chance == 0 && fixture.settings.jump_delay_ms == 100 && fixture.settings.mode == 1);
    assert(fixture.apply_config_settings("{}") && fixture.revisions == revisions + 2);
    for (int mode : {0, 1}) {
        config candidate; candidate.mode = mode;
        for (profile value : {profile::immediate, profile::balanced, profile::controlled}) {
            apply_profile(candidate, value);
            assert(candidate.mode == mode && candidate.only_combat);
            assert(selected_profile(candidate) == value && normalize(candidate));
        }
        const auto before = candidate;
        apply_profile(candidate, profile::custom);
        assert(same_config(candidate, before));
        --candidate.chance; assert(selected_profile(candidate) == profile::custom);
    }
    config defaults;
    assert(defaults.mode == 0 && defaults.chance == 100 && defaults.jump_delay_ms == 0 && defaults.only_combat);
    std::cout << "Jump settings compatibility, presets, input-route preservation and atomic import passed.\n";
}
