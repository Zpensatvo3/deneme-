// Exercise parsing/normalization without reading or writing the user's preferences.
#include "../src/gui/ui_state.cpp"
#include <iostream>
#include <sstream>

int main()
{
    int failures = 0;
    auto check = [&](bool ok, const char* message) { if (!ok) { std::cerr << message << '\n'; ++failures; } };
    // Fake time only: do not read/write the user's preferences or sleep.
    save_schedule save;
    const auto start = save_schedule::clock::time_point{};
    using namespace std::chrono_literals;
    check(!save.ready(start, true, false), "clean state must not save even on force");
    save.changed(start);
    check(!save.ready(start + 649ms, false, false), "save must debounce edits");
    save.changed(start + 600ms);
    check(!save.ready(start + 1s, false, false), "continuous edits must restart debounce");
    check(save.ready(start + 1250ms, false, false), "idle changes must eventually save");
    check(!save.ready(start + 1250ms, false, true), "active dragging must not write to disk");
    save.attempted(start + 1250ms);
    check(!save.ready(start + 1260ms, false, false), "failed save must not retry next frame");
    check(save.ready(start + 3250ms, false, false), "failed save must retry after backoff");
    check(save.ready(start + 1260ms, true, true), "explicit save/shutdown must bypass delay");
    save.saved();
    check(!save.ready(start + 10s, false, false), "successful save must stop retries");
    save.changed(start + 1400ms);
    check(save.ready(start + 2050ms, false, false), "successful save must clear the retry backoff");
    float value = 12.f;
    int index = 3;
    check(!read_number("{\"x\":nan}", "x", value) && value == 12.f, "NaN must not mutate preferences");
    check(!read_number("{\"x\":inf}", "x", value), "infinity must be rejected");
    check(!read_number("{\"x\":12garbage}", "x", value), "numeric suffix garbage must be rejected");
    check(read_number("{\"x\": 0.75,\"y\":1}", "x", value) && value == .75f, "valid JSON number must parse");
    check(!read_integer("{\"theme\":1.5}", "theme", index) && index == 3, "fractional theme must be rejected");
    check(!read_integer("{\"theme\":1e30}", "theme", index), "integer overflow must be rejected");
    check(read_integer("{\"theme\":7}", "theme", index) && index == 7, "valid theme must parse");
    gui::ui_state::preferences test{};
    test.window_size = ImVec2(std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity());
    test.window_position = ImVec2(std::numeric_limits<float>::quiet_NaN(), -std::numeric_limits<float>::infinity());
    normalize(test);
    check(test.window_size.x == 1280 && test.window_size.y == 620, "invalid dimensions must normalize");
    check(test.menu_background.selected == gui::background::style::classic, "old preferences retain classic background");
    test.menu_background = {static_cast<gui::background::style>(999), NAN, INFINITY};
    normalize(test);
    check(test.menu_background.selected == gui::background::style::classic &&
        test.menu_background.opacity == .85f && test.menu_background.dimming == .48f,
        "invalid background preferences normalize safely");
    check(test.window_position.x == 0 && test.window_position.y == 0, "invalid position must normalize");
    test.hud_layout[0].x = std::numeric_limits<float>::quiet_NaN();
    test.hud_layout[1].scale = 10.f;
    test.hud_layout[2].scale = .5f;
    normalize(test);
    check(test.hud_layout[0].x == 10.f, "HUD coordinates must remain finite");
    check(test.hud_layout[1].scale == 2.f, "HUD scale must clamp");
    check(test.hud_layout[2].scale == 1.2f, "Spotify scale must stay fixed");
    test.has_hud_layout = true;
    test.hud_layout[1] = {42, 108, 1.15f, false};
    std::ostringstream encoded;
    write_hud_preferences(encoded, test);
    encoded << "\"version\":1}";
    bool has_layout = false, visible = true;
    float x = 0, y = 0, scale = 0;
    check(read_boolean(encoded.str(), "has_hud_layout", has_layout) && has_layout, "HUD saved marker must roundtrip");
    check(read_number(encoded.str(), "hud_arraylist_x", x) && x == 42 &&
        read_number(encoded.str(), "hud_arraylist_y", y) && y == 108, "HUD right/top anchors must roundtrip");
    check(read_number(encoded.str(), "hud_arraylist_scale", scale) && std::fabs(scale - 1.15f) < .0001f,
        "HUD scale must roundtrip");
    check(read_boolean(encoded.str(), "hud_arraylist_visible", visible) && !visible, "HUD visibility must roundtrip");
    if (failures) return 1;
    std::cout << "UI preference tests passed.\n";
}
