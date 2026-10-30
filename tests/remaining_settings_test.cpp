#define main existing_config_fields_round_trip_test
#include "config_fields_test.cpp"
#undef main
#include "../src/modules/combat/settings/shield_breaker_settings.hpp"
#include "../src/modules/combat/settings/anti_bot_settings.hpp"
#include "../src/modules/combat/settings/auto_totem_settings.hpp"
#include "../src/modules/misc/settings/fast_exp_settings.hpp"
#include "../src/modules/visual/settings/player_esp_settings.hpp"
#include "../src/modules/visual/settings/storage_esp_settings.hpp"
#include "../src/modules/visual/settings/item_esp_settings.hpp"
#include "../src/modules/visual/settings/block_esp_settings.hpp"
#include "../src/modules/visual/settings/radar_settings.hpp"
#include "../src/modules/visual/settings/spotify_hud_settings.hpp"
#include "../src/modules/visual/settings/chams_settings.hpp"
#include "../src/modules/visual/settings/hit_particles_settings.hpp"
#include "../src/modules/visual/settings/jump_circles_settings.hpp"
#include "../src/modules/visual/visual_effects_logic.hpp"
#include "../src/modules/visual/cape_design.hpp"
#include "../src/modules/visual/settings/cape_settings.hpp"
#include "../src/modules/visual/settings/wings_settings.hpp"
#include "../src/modules/render/settings/watermark_settings.hpp"
#include "../src/modules/render/settings/arraylist_settings.hpp"
#include "../src/modules/combat/click_aim_config.hpp"
#include "../src/modules/combat/legit_aim_config.hpp"
#include <limits>
#include <cassert>
#include <set>
#include "../src/modules/core/settings_store.hpp"
#include "support/dto_fixture.hpp"

template<class Model>
class settings_fixture final : public modules::config_fixture
{
public:
    settings_fixture() : config_fixture("utility_fixture") {}
    dto_fixture<Model> settings;
    void visit_config(modules::config_visitor& visitor) override
    {
        settings.visit(visitor, Model::schema());
    }
};

template<class Model>
void check_contract(std::initializer_list<const char*> expected)
{
    settings_fixture<Model> owner;
    const Model initial;
    const auto encoded = owner.export_config_settings();
    assert(owner.validate_config_settings(encoded) && owner.settings.read() == initial);
    assert(owner.apply_config_settings(encoded) && owner.settings.read() == initial);
    assert(owner.apply_config_settings("{}") && owner.settings.read() == initial);
    std::set<std::string> keys;
    for (const auto& d : Model::schema())
    {
        assert(keys.insert(d.key).second);
        assert(encoded.find(std::string("\"") + d.key + "\":") != std::string::npos);
        const auto saved = owner.export_config_settings();
        const std::string invalid = std::string("{\"") + d.key + "\":\"bad\"}";
        assert(!owner.validate_config_settings(invalid));
        assert(!owner.apply_config_settings(invalid));
        assert(owner.export_config_settings() == saved);
        if (d.type == modules::settings::control::integer)
        {
            for (const char* value : {"0.5", "2147483648", "-2147483649", "1e999"})
            {
                assert(!owner.apply_config_settings(std::string("{\"") + d.key + "\":" + value + "}"));
                assert(owner.export_config_settings() == saved);
            }
        }
    }
    assert(keys == std::set<std::string>(expected.begin(), expected.end()));
}

class click_aim_fixture final : public modules::config_fixture
{
public:
    click_aim_fixture() : config_fixture("click_aim_fixture") {}
    click_aim_engine::config settings;
    void visit_config(modules::config_visitor& v) override { click_aim_engine::visit_config(v, settings); }
};

int main()
{
    using namespace module_settings;
    check_contract<hit_particles>({"style","count","lifetime","size","opacity","theme_color","color","glow"});
    check_contract<jump_circles>({"style","trigger","radius","lifetime","opacity","theme_color","color","glow"});
    check_contract<cape>({"style","tint_art","length","width","animate","motion","physics","opacity","theme_color","color","glow"});
    check_contract<wings>({"style","detailed_art","tint_art","size","opacity","animate","speed","physics","theme_color","color","glow"});
    {
        settings_fixture<cape> cloth;
        for (int style=0;style<10;++style) {
            assert(cloth.apply_config_settings("{\"style\":"+std::to_string(style)+"}"));
            assert(cloth.settings.read().style==style);
            assert(cape_design::strokes(style).size()<=24);
            for (const auto& segment:cape_design::strokes(style))
                for (const auto p:{segment.a,segment.b})
                    assert(p.x>=0 && p.x<=1 && p.y>=0 && p.y<=1);
            for (int y=0;y<=14;++y) for (int x=0;x<=12;++x) {
                const auto pattern=cape_design::sample(style,x/12.,y/14.);
                assert(std::isfinite(pattern.shade) && pattern.shade>=0 && pattern.shade<=1);
                assert(std::isfinite(pattern.ember) && pattern.ember>=0 && pattern.ember<=1);
            }
        }
        const auto encoded=cloth.export_config_settings();
        assert(!cloth.apply_config_settings(R"({"style":10})"));
        assert(cloth.export_config_settings()==encoded);
    }
    {
        visual_effects_logic::jump_detector jumps;
        assert(jumps.sample(true,false)==0);
        assert(jumps.sample(false,true)==1);
        assert(jumps.sample(false,true)==0);
        assert(jumps.sample(false,false)==0);
        assert(jumps.sample(true,false)==2);
        assert(jumps.sample(true,false)==0);
        jumps.reset(); assert(jumps.sample(false,true)==0);
        visual_effects_logic::hit_detector hits;
        assert(!hits.sample(10,42,8,18.f,0)); // Existing hurt state is not a hit.
        assert(!hits.sample(0,42,0,18.f,.1));
        assert(hits.pending(.2));
        assert(hits.sample(0,42,10,16.f,.2));
        assert(!hits.sample(1,42,9,16.f,.21)); // Same damage, another frame.
        assert(!hits.sample(0,42,8,16.f,.3));
        assert(!hits.sample(9,42,10,14.f,.9)); // Expired acknowledgement.
        assert(!hits.sample(0,43,10,12.f,1)); // Target switch does not inherit pending hit.
        assert(!hits.sample(1,43,10,12.f,1.1));
        assert(!hits.sample(2,43,0,std::numeric_limits<float>::quiet_NaN(),1.2));
        assert(!hits.pending(1.2));
        // Cooldown reset must be observed before the crosshair is refreshed.
        hits.reset();
        assert(!hits.sample(10,42,0,20.f,0));
        assert(hits.observe_attack(0,.1) && hits.pending(.1));
        assert(hits.sample(0,42,10,18.f,.11));
        assert(!hits.sample(0,42,10,18.f,.12));
        // Network damage observed one frame before the local reset still works.
        hits.reset();
        assert(!hits.sample(8,42,0,20.f,0));
        assert(!hits.sample(9,42,10,18.f,.1));
        assert(hits.sample(0,42,9,18.f,.12));
        assert(!hits.sample(1,42,9,18.f,.13));
        // Delayed acknowledgement, stale damage, and unrelated target damage.
        hits.reset();
        assert(!hits.sample(10,42,0,20.f,0));
        assert(!hits.sample(0,42,0,20.f,.1));
        assert(hits.sample(8,42,10,18.f,.55));
        hits.reset();
        assert(!hits.sample(10,42,0,20.f,0));
        assert(!hits.sample(11,42,10,18.f,.1));
        assert(!hits.sample(0,42,8,18.f,.3));
        assert(!hits.sample(1,43,10,18.f,.31));
        assert(!hits.pending(.31));
        assert(!hits.sample(1,43,0,18.f,.2)); // Clock regression invalidates observations.
    }
    check_contract<anti_bot>({"check_player_list","spawn_grace_ticks","missing_list_ticks"});
    assert(anti_bot_default_enabled && anti_bot{}.check_player_list);
    assert(anti_bot{}.spawn_grace_ticks == 40 && anti_bot{}.missing_list_ticks == 40);
    check_contract<shield_breaker>({"switch_delay","return_delay","axe_slot"});
    check_contract<auto_totem>({"mode","delay","hotbar","totem_slot","auto_switch","force_totem","auto_open","stay_open_for"});
    assert(auto_totem{}.delay == 0 && auto_totem{}.hotbar && auto_totem{}.totem_slot == 1 &&
        !auto_totem{}.auto_switch && !auto_totem{}.force_totem && !auto_totem{}.auto_open);
    check_contract<fast_exp>({"min_cps","max_cps","cps"});
    check_contract<player_esp>({"esp_mode","box_thickness","fill_opacity","show_names","show_health","show_distance","fov","use_game_fov","use_theme_color","esp_color"});
    check_contract<storage_esp>({"show_labels","show_distance","box_style"});
    check_contract<item_esp>({"show_boxes","show_names","show_count","show_distance","use_theme_color","color","line_thickness","fill_opacity"});
    check_contract<block_esp>({"line_thickness","fill_opacity","selected_blocks"});
    check_contract<radar>({"size","range","position_x","position_y","background_opacity","rotate_with_player","show_names","show_distance_rings","show_cardinals","show_player_count"});
    check_contract<spotify_hud>({"position_x","position_y","scale","background_opacity","show_artwork","show_progress","show_visualizer","show_brand","show_lyrics","lyric_lines","enable_lyrics","show_when_idle"});
    check_contract<chams>({"opacity","glow","glow_strength","glow_radius","through_walls","theme_color","ignore_friends","visible_color","hidden_color"});
    check_contract<watermark>({"scale","position_x","position_y"});
    check_contract<arraylist>({"scale","position_x","position_y","bar_glow","text_shadow","bg_opacity","bar_width","show_keybinds"});

    settings_fixture<block_esp> blocks;
    assert(blocks.apply_config_settings(R"({"selected_blocks":["minecraft:stone","minecraft:diamond_ore"]})"));
    assert(blocks.settings.read().selected_blocks == (std::vector<std::string>{"minecraft:stone","minecraft:diamond_ore"}));
    const auto block_saved = blocks.export_config_settings();
    assert(!blocks.apply_config_settings(R"({"selected_blocks":["minecraft:stone",12],"range":4})"));
    assert(blocks.export_config_settings() == block_saved);

    settings_fixture<chams> colors;
    assert(colors.apply_config_settings(R"({"visible_color":[-1,2,0.5,0.8],"range":200})"));
    const auto c = colors.settings.read();
    assert(c.visible_color[0] == 0 && c.visible_color[1] == 1 && c.visible_color[2] == .5f);
    assert(colors.export_config_settings().find("\"range\"") == std::string::npos);
    // Old range values import harmlessly but are never re-exported or applied.
    settings_fixture<storage_esp> storage;
    assert(storage.apply_config_settings(R"({"range":4,"show_labels":false})"));
    assert(!storage.settings.read().show_labels);
    assert(storage.export_config_settings().find("\"range\"") == std::string::npos);
    assert(blocks.apply_config_settings(R"({"range":3})"));
    assert(blocks.export_config_settings().find("\"range\"") == std::string::npos);
    const auto color_saved = colors.export_config_settings();
    assert(!colors.apply_config_settings(R"({"visible_color":[0,0,0]})"));
    assert(!colors.apply_config_settings(R"({"hidden_color":[0,0,1e999,1]})"));
    assert(colors.export_config_settings() == color_saved);

    settings_fixture<fast_exp> exp;
    assert(exp.apply_config_settings(R"({"cps":24})"));
    assert(exp.settings.read().min_cps == 22 && exp.settings.read().max_cps == 26 && exp.settings.read().legacy_cps == 0);
    assert(exp.apply_config_settings(R"({"min_cps":10,"max_cps":12,"cps":30})"));
    assert(exp.settings.read().min_cps == 10 && exp.settings.read().max_cps == 12);
    assert(exp.apply_config_settings(R"({"min_cps":1,"max_cps":50})"));
    assert(exp.settings.read().min_cps == 1 && exp.settings.read().max_cps == 50);
    settings_fixture<auto_totem> totem;
    assert(totem.apply_config_settings(R"({"mode":1,"delay":20,"hotbar":false,"totem_slot":9,"auto_switch":true,"force_totem":true,"auto_open":true,"stay_open_for":18})"));
    assert(totem.settings.read().mode == 1 && totem.settings.read().delay == 20 && !totem.settings.read().hotbar &&
        totem.settings.read().totem_slot == 9 && totem.settings.read().auto_switch && totem.settings.read().force_totem &&
        totem.settings.read().auto_open && totem.settings.read().stay_open_for == 18);
    settings_fixture<radar> radar_overlay;
    assert(radar_overlay.apply_config_settings(R"({"position_x":2100,"position_y":1300})"));
    assert(radar_overlay.settings.read().position_x == 2100 && radar_overlay.settings.read().position_y == 1300);

    click_aim_fixture aim;
    assert(aim.apply_config_settings(R"({"aim_regions":[0,7,15,59]})"));
    assert(aim.settings.regions == std::vector<int>({0,7,15,59}));
    const auto aim_saved = aim.export_config_settings();
    assert(!aim.apply_config_settings(R"({"aim_regions":[60],"fov":45})"));
    assert(aim.export_config_settings() == aim_saved);
    auto color = chams{};
    color.visible_color[2] = std::numeric_limits<float>::infinity();
    assert(!modules::settings::normalize(color, chams::schema()));
    auto legit = legit_aim_engine::config{};
    legit.point = static_cast<legit_aim_engine::aim_point>(3);
    assert(!modules::settings::normalize(legit, legit_aim_engine::schema()));
    std::cout << "Remaining module contracts: stable keys, defaults, colors, lists, sparse enums, legacy EXP keys and atomic imports passed\n";
}
