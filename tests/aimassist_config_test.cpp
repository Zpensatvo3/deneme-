#define main existing_config_fields_round_trip_test
#include "config_fields_test.cpp"
#undef main
#include "../src/modules/combat/legit_aim_config.hpp"
#include <cassert>

class aim_fixture final : public modules::module
{
public:
    aim_fixture() : module("aim_assist") {}
    legit_aim_engine::config settings;
    int resets = 0;
    void visit_config(modules::config_visitor& v) override
    {
        if (legit_aim_engine::visit_config(v, settings)) ++resets;
    }
};
int main()
{
    using namespace legit_aim_engine;
    aim_fixture fixture;
    assert(fixture.settings.strength == 42.f);
    const auto before = fixture.export_config_settings();
    assert(fixture.validate_config_settings(before) && fixture.resets == 0);
    assert(fixture.apply_config_settings(before) && fixture.resets == 1);
    for (int point : {0, 1, 4}) {
        assert(fixture.apply_config_settings("{\"aim_point\":" + std::to_string(point) + "}"));
        assert(static_cast<int>(fixture.settings.point) == point);
    }
    assert(fixture.apply_config_settings(R"({"strength":0,"fov":60,"max_distance":4.2,"weapon_only":false})"));
    assert(fixture.settings.strength == 0 && !fixture.settings.weapon_only);
    assert(fixture.apply_config_settings(R"({"strength":150,"fov":-10,"max_distance":100})"));
    assert(fixture.settings.strength == 100 && fixture.settings.fov == 20 && fixture.settings.max_distance == 5);
    const auto saved = fixture.export_config_settings();
    const int resets = fixture.resets;
    for (const auto* invalid : {R"({"aim_point":2})", R"({"aim_point":3})", R"({"aim_point":-1})",
        R"({"aim_point":99})", R"({"strength":1e999})", R"({"strength":25,"weapon_only":"bad"})",
        R"({"fov":30,"max_distance":"bad"})"}) {
        assert(!fixture.validate_config_settings(invalid));
        assert(!fixture.apply_config_settings(invalid));
        assert(fixture.export_config_settings() == saved && fixture.resets == resets);
    }
    assert(fixture.apply_config_settings("{}") && fixture.resets == resets + 1);
    std::cout << "Aim config fields, sparse enum values, zero strength, atomic import and resets passed.\n";
}
