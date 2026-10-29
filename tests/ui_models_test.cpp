// Pure UI model regressions share one executable; the rendered UI has its own integration test.
#include "../src/gui/theme_palette.hpp"
#include <iostream>
#include <limits>
#include "../src/modules/render/arraylist_layout.hpp"
#include <cmath>
#include <string>
#include <vector>
#include "../src/ui/hud_editor_layout.hpp"
#include "../src/ui/config_page_model.hpp"
#include <cassert>
#include <cstdio>
#include "../src/ui/module_categories.hpp"
#include "../src/ui/keyboard_layout.hpp"
#include <set>
#include "../src/ui/dock_layout.hpp"
#include "../src/ui/menu_background.hpp"
#include "../src/services/game/menu_input_policy.hpp"

namespace theme_palette_tests
{
namespace
{
    int failures = 0;
    void check(bool condition, const char* message)
    {
        if (!condition) { std::cerr << message << '\n'; ++failures; }
    }
    float distance(gui::theme::color a, gui::theme::color b)
    {
        return std::fabs(a.r - b.r) + std::fabs(a.g - b.g) + std::fabs(a.b - b.b);
    }
}
int run()
{
    using namespace gui::theme;
    for (int theme = 0; theme < count; ++theme)
    {
        check(distance(sample(theme, 0), sample(theme, 1)) < .0001f, "palette loop must close");
        check(distance(sample(theme, -0.2), sample(theme, .8)) < .0001f, "negative phases must wrap");
        for (double phase = 0; phase < 1; phase += .01)
        {
            const color c = sample(theme, phase);
            check(c.r >= 0 && c.r <= 1 && c.g >= 0 && c.g <= 1 && c.b >= 0 && c.b <= 1, "palette out of range");
        }
    }
    transition animation;
    animation.evaluate(0, 100);
    animation.evaluate(2, 100);
    const color midway = animation.evaluate(2, 100.25, 90);
    check(distance(midway, animation.evaluate(4, 100.25, 90)) < .0001f, "interrupted theme change must not jump");
    animation.evaluate(6, 100.4, 90);
    const color finished = animation.evaluate(6, 101.1, 90);
    check(distance(finished, sample(6, std::fmod(101.1, cycle_seconds) / cycle_seconds + 90 * .0035)) < .0001f,
        "transition must reach the requested palette");
    check(distance(animation.evaluate(3, 110, 90, true), animation.evaluate(3, 130, 90, true)) < .0001f,
        "reduce motion must freeze animated colors");
    check(distance(animation.evaluate(-100, 130, 0, true), sample(0, 0)) < .0001f, "invalid theme must clamp");
    check(distance(animation.evaluate(0, 140, 0, true), animation.evaluate(0, 140, 180, true)) > .1f,
        "different ArrayList rows must have a visible gradient");
    const color uptime = animation.evaluate(0, 1e9);
    const color later = animation.evaluate(0, 1e9 + .1);
    check(distance(uptime, later) > .000001f && distance(uptime, later) < .02f, "long uptime must keep smooth precision");
    check(std::isfinite(animation.evaluate(0, std::numeric_limits<double>::quiet_NaN()).r), "invalid time must stay finite");
    if (failures) return 1;
    std::cout << "theme palette tests passed.\n";
    return 0;
}
}

namespace arraylist_layout_tests
{
namespace
{
    int failures = 0;

    void check(bool condition, const char* what)
    {
        if (condition) return;
        std::cerr << "FAIL: " << what << '\n';
        ++failures;
    }

    bool near(float left, float right, float tolerance = 0.001f)
    {
        return std::fabs(left - right) <= tolerance;
    }

    modules::arraylist_layout::geometry metrics()
    {
        modules::arraylist_layout::geometry value{};
        value.font_size = 18.0f;
        value.padding_x = 7.0f;
        value.padding_y = 3.0f;
        value.bar_width = 3.0f;
        value.keybind_gap = 6.0f;
        value.chip_padding = 4.0f;
        return value;
    }

    modules::arraylist_layout::row make_row(std::string label, float text_width,
        std::string keybind = {}, float keybind_width = 0.0f, float visibility = 1.0f)
    {
        modules::arraylist_layout::row row{};
        row.key = label;
        row.label = std::move(label);
        row.keybind = std::move(keybind);
        row.text_width = text_width;
        row.keybind_width = keybind_width;
        row.visibility = visibility;
        return row;
    }

    void test_sorting()
    {
        std::vector<modules::arraylist_layout::row> rows;
        rows.push_back(make_row("Sprint", 40.0f));
        rows.push_back(make_row("Aim Assist", 90.0f));
        rows.push_back(make_row("Brake", 55.0f));

        modules::arraylist_layout::sort_rows(rows);

        check(rows[0].label == "Aim Assist", "longest label sorts first");
        check(rows[1].label == "Brake", "middle width sorts second");
        check(rows[2].label == "Sprint", "shortest label sorts last");

        // Equally wide labels fall back to alphabetical order.
        std::vector<modules::arraylist_layout::row> ties;
        ties.push_back(make_row("Sprint", 40.0f));
        ties.push_back(make_row("Anchor", 40.0f));
        ties.push_back(make_row("Radar", 40.0f));

        modules::arraylist_layout::sort_rows(ties);
        check(ties[0].label == "Anchor", "tie sorts alphabetically (first)");
        check(ties[1].label == "Radar", "tie sorts alphabetically (second)");
        check(ties[2].label == "Sprint", "tie sorts alphabetically (third)");
    }

    void test_row_width()
    {
        const auto geometry = metrics();

        const auto plain = make_row("Sprint", 40.0f);
        check(near(modules::arraylist_layout::row_width(plain, geometry), 57.0f),
            "plain row is text + 2*padding_x + bar");

        const auto bound = make_row("Sprint", 40.0f, "R", 10.0f);
        // 40 text + 7*2 padding + 3 bar + 6 gap + 10 chip + 4*2 chip padding
        check(near(modules::arraylist_layout::row_width(bound, geometry), 81.0f),
            "keybind row adds gap + chip + 2*chip_padding");

        // An empty keybind must not widen the row even if a stale width is present.
        const auto unbound = make_row("Sprint", 40.0f, "", 25.0f);
        check(near(modules::arraylist_layout::row_width(unbound, geometry), 57.0f),
            "empty keybind contributes no width");
    }

    void test_build()
    {
        const auto geometry = metrics();
        const float line_height = geometry.line_height();

        std::vector<modules::arraylist_layout::row> rows;
        rows.push_back(make_row("Aim Assist", 90.0f, {}, 0.0f, 1.0f));
        rows.push_back(make_row("Sprint", 40.0f, "R", 10.0f, 0.5f));

        const auto layout = modules::arraylist_layout::build(rows, geometry);
        const float right_edge = 1000.0f;

        check(layout.drawable, "layout with visible rows is drawable");
        // widest row: 90 text + 7*2 padding + 3 bar = 107
        check(near(layout.width, 107.0f), "block width matches the widest row");
        check(near(layout.height, line_height * 1.0f + line_height * 0.5f),
            "block height sums line_height * visibility");
        check(layout.row_top.size() == 2 && layout.row_left.size() == 2,
            "per row offsets are produced");
        check(near(layout.row_top[0], 0.0f), "first row starts at the block top");
        check(near(layout.row_top[1], line_height), "second row stacks below the first");
        check(near(layout.row_left[0], 107.0f), "widest row reaches the block's left edge");
        check(near(layout.row_left[1], 81.0f), "narrower row starts further right");

        // The renderer places each row at right_edge - row_left. Guard the contract
        // that keeps the staircase from collapsing onto the right edge: the widest
        // row must reach exactly the block's left side.
        for (std::size_t index = 0; index < layout.row_left.size(); ++index)
        {
            const float left = right_edge - layout.row_left[index];
            check(left >= right_edge - layout.width - 0.001f,
                "no row extends left of the block");
            check(layout.row_left[index] > 0.0f, "every row has a positive width");
        }
        check(near(right_edge - layout.row_left[0], right_edge - layout.width),
            "widest row is flush with the block's left edge");
        check(layout.row_left[0] >= layout.row_left[1],
            "wider rows reach further left");

        // Offsets must stay monotonic so the draw passes can index them directly.
        for (std::size_t index = 1; index < layout.row_top.size(); ++index)
            check(layout.row_top[index] >= layout.row_top[index - 1], "row tops are monotonic");

        std::vector<modules::arraylist_layout::row> hidden;
        hidden.push_back(make_row("Sprint", 40.0f, {}, 0.0f, 0.0f));
        check(!modules::arraylist_layout::build(hidden, geometry).drawable,
            "fully hidden rows are not drawable");

        check(!modules::arraylist_layout::build({}, geometry).drawable,
            "empty input is not drawable");
    }

    void test_clamp_anchor()
    {
        const float screen_w = 1920.0f;
        const float screen_h = 1080.0f;
        const float margin = 4.0f;

        float inside_x = 100.0f;
        float inside_y = 50.0f;
        modules::arraylist_layout::clamp_anchor(inside_x, inside_y, 200.0f, 150.0f, screen_w, screen_h, margin);
        check(near(inside_x, 100.0f) && near(inside_y, 50.0f), "in-bounds anchor is untouched");

        float overflow_x = 5000.0f;
        float overflow_y = -40.0f;
        modules::arraylist_layout::clamp_anchor(overflow_x, overflow_y, 200.0f, 150.0f, screen_w, screen_h, margin);
        check(near(overflow_x, screen_w - 200.0f - margin), "right overflow is pulled back on screen");
        check(near(overflow_y, margin), "negative top is pulled to the margin");
        check(overflow_x >= margin && overflow_y >= margin, "clamped anchor stays on screen");

        float bottom_x = 10.0f;
        float bottom_y = 5000.0f;
        modules::arraylist_layout::clamp_anchor(bottom_x, bottom_y, 200.0f, 150.0f, screen_w, screen_h, margin);
        check(near(bottom_y, screen_h - 150.0f - margin), "bottom overflow is pulled back on screen");

        // A block wider than the screen settles on the margin instead of looping.
        float tiny_x = 0.0f;
        float tiny_y = 0.0f;
        modules::arraylist_layout::clamp_anchor(tiny_x, tiny_y, 4000.0f, 2000.0f, screen_w, screen_h, margin);
        check(near(tiny_x, margin) && near(tiny_y, margin), "oversized block settles on the margin");

        // Degenerate inputs must not produce nonsense.
        float zero_x = 12.0f;
        float zero_y = 12.0f;
        modules::arraylist_layout::clamp_anchor(zero_x, zero_y, 0.0f, 0.0f, screen_w, screen_h, margin);
        check(near(zero_x, 12.0f) && near(zero_y, 12.0f), "zero sized block is ignored");
    }

    void test_badge_sorting_and_gaps()
    {
        auto geometry = metrics();
        geometry.row_gap = 2.0f;
        std::vector<modules::arraylist_layout::row> rows = {
            make_row("Aim Assist", 90.0f),
            make_row("Radar", 55.0f, "LBUTTON", 40.0f),
            make_row("Sprint", 40.0f)
        };
        modules::arraylist_layout::sort_rows(rows, geometry);
        check(rows.front().label == "Radar", "badge width participates in visible row sorting");
        const auto sorted = modules::arraylist_layout::build(rows, geometry);
        for (std::size_t index = 1; index < sorted.row_left.size(); ++index)
            check(sorted.row_left[index - 1] >= sorted.row_left[index], "visible widths descend including badges");
        check(near(sorted.height, 3.0f * geometry.line_height() + 4.0f), "gaps only separate rows");

        rows[1].visibility = 0.5f;
        const auto fading = modules::arraylist_layout::build(rows, geometry);
        check(near(fading.height, 2.5f * geometry.line_height() + 2.0f), "adjacent gaps collapse with a fading row");
        check(near(fading.row_top[1], geometry.line_height() + 1.0f), "fading row starts after the shrinking gap");

        rows = { make_row("Sprint", 40.0f), make_row("Radar", 55.0f, {}, 0.0f, 0.0f) };
        check(near(modules::arraylist_layout::build(rows, geometry).height, geometry.line_height()),
            "a hidden trailing row leaves no trailing gap");
    }
}

int run()
{
    test_sorting();
    test_row_width();
    test_build();
    test_clamp_anchor();
    test_badge_sorting_and_gaps();

    if (failures > 0)
    {
        std::cerr << failures << " arraylist layout assertion(s) failed.\n";
        return 1;
    }

    std::cout << "arraylist layout tests passed.\n";
    return 0;
}
}

namespace hud_editor_layout_tests
{
int run()
{
    using namespace gui::hud_editor_layout;
    int failures = 0;
    auto check = [&](bool ok, const char* message) { if (!ok) { std::cerr << message << '\n'; ++failures; } };
    const point canvas{ 800, 600 }, size{ 100, 50 };
    auto position = clamp({ 1000, -20 }, size, canvas);
    check(position.x == 692 && position.y == 8, "screen edges must clamp");
    position = clamp({ 20, 20 }, { 1000, 900 }, canvas);
    check(position.x == 8 && position.y == 8, "oversized overlays must not invert clamp bounds");
    position = clamp({ std::numeric_limits<float>::quiet_NaN(), 20 }, size, canvas);
    check(position.x == 8, "invalid saved coordinates must stay finite");
    auto aligned = place({ 348, 273 }, size, canvas, true, true, {});
    check(aligned.position.x == 350 && aligned.guide_x == 400, "screen center alignment must use overlay center");
    check(aligned.position.y == 275 && aligned.guide_y == 300, "vertical center must snap");
    aligned = place({ 101, 137 }, size, canvas, true, false, {{ 100, 200, 100, 50 }});
    check(aligned.position.x == 100 && aligned.guide_x >= 0, "neighbour edges must align");
    aligned = place({ 101, 137 }, size, canvas, true, true, {}, true);
    check(aligned.position.x == 101 && aligned.position.y == 137 && aligned.guide_x == -1, "Alt must bypass snapping");
    aligned = place({ 117, 137 }, size, canvas, true, true, {});
    check(aligned.position.x == 120 && aligned.position.y == 136, "8 pixel grid must snap");
    if (failures) return 1;
    std::cout << "HUD editor layout tests passed.\n";
    return 0;
}
}

namespace config_page_model_tests
{
int run()
{
    namespace model = gui::config_page::model;
    configs::snapshot library;
    library.entries = {
        { 1, "Survival", "Wolf", "Calm HUD", 4, 80, "2026-10-01", "2026-10-12", true },
        { 2, "Arena", "Player", "Combat profile", 8, 210, "2026-10-02", "2026-10-10", false },
        { 3, "Builder", "Wolf", "Minimal layout", 2, 12, "2026-10-13", "", true },
        { 4, "Explorer", "Alex", "Night profile", 3, 20, "2026-10-14", "", false }
    };
    const std::unordered_set<int> favourites{ 2 };
    auto list = model::filter(library, model::collection::community, "", favourites, model::order::newest);
    assert(list.size() == 2 && list[0]->id == 4 && list[1]->id == 2);
    for (const auto* entry : list) assert(!entry->mine);
    list = model::filter(library, model::collection::community, "  COMBAT  ", favourites, model::order::popular);
    assert(list.size() == 1 && list[0]->id == 2);
    list = model::filter(library, model::collection::mine, "wolf", favourites, model::order::name);
    assert(list.size() == 2 && list[0]->id == 3);
    for (const auto* entry : list) assert(entry->mine);
    list = model::filter(library, model::collection::favourites, "", favourites, model::order::popular);
    assert(list.size() == 1 && list[0]->id == 2);
    assert(model::filter(library, model::collection::mine, "Arena", favourites, model::order::name).empty());
    assert(model::filter(library, model::collection::community, "Survival", favourites, model::order::name).empty());
    assert(model::trim("  \n \t").empty());
    char short_text[4]{};
    model::copy_utf8(short_text, "ab\xc4\xb1" "c");
    assert(std::string(short_text) == "ab");
    model::copy_utf8(short_text, "a\xc4\xb1" "c");
    assert(std::string(short_text) == "a\xc4\xb1");
    assert(model::date("2026-10-13T12:00:00") == "2026-10-13");
    assert(model::date("") == "--");
    std::puts("Config collection, description search, sorting, trim and UTF-8 boundary checks passed.");
    return 0;
}
}

namespace keybinds_presentation_tests
{
int run()
{
    using gui::module_categories::display;
    std::set<std::string_view> groups;
    for (auto source : {"Combat", "Movement", "Player", "World", "Misc", "Friends", "Visual", "Render", "Vape", "Client"})
        if (auto group = display(source); !group.empty()) groups.insert(group);
    assert(groups == (std::set<std::string_view>{"Combat", "Misc", "Visuals", "Hud", "Vape", "Client"}));
    for (auto source : {"Movement", "Player", "World", "Misc"}) assert(display(source) == "Misc");
    assert(display("Friends").empty());
    assert(display("Custom group") == "Custom group"); // extension modules stay usable
    for (float width : {420.f, 600.f, 700.f, 1000.f}) {
        const auto layout = gui::keyboard_layout::measure(width);
        assert(layout.unit >= 29.f && layout.width >= width - .01f);
        assert(layout.scroll == (width < 667.f));
        for (const auto& key : platform::keys::keyboard) {
            assert(key.x * layout.unit + key.w * layout.unit <= layout.width + .01f);
            assert(key.y * layout.unit + key.h * layout.unit <= layout.height + .01f);
            assert(gui::keyboard_layout::font_size(key) >= 12.5f);
        }
    }
    std::cout << "PASS unified keybind categories and readable 104-key layout\n";
    return 0;
}
}

namespace dock_layout_tests
{
int run()
{
    using namespace gui::dock_layout;
    for (float width : {480.f, 800.f, 1280.f, 1920.f, 2560.f}) {
        const box work{120, 40, width, 900};
        for (float drag : {-2000.f, 0.f, 210.f, 2000.f}) {
            const auto before = main_panel(work, drag, 30);
            const auto drawer = settings_panel(work, before);
            const auto after = main_panel(work, drag, 30);
            assert(before.x == after.x && before.y == after.y && before.width == after.width);
            assert(drawer.x >= work.x + margin && drawer.x + drawer.width <= work.x + work.width - margin + .001f);
            assert(after.x >= work.x + margin && after.x + after.width <= work.x + work.width - margin + .001f);
        }
    }
    const box wide{0, 0, 1920, 1080}, compact{0, 0, 1280, 720};
    const auto main = main_panel(wide);
    assert(settings_panel(wide, main).x == main.x + main.width + gap);
    assert(preview_fits(wide, main, 260));
    const auto small = main_panel(compact);
    assert(settings_panel(compact, small).x < small.x + small.width);
    assert(!preview_fits(compact, small, 260));
    auto at_edge = positioned_main(wide, 100000.f, -100000.f);
    assert(at_edge.x + at_edge.width == wide.width - margin && at_edge.y == margin);
    const auto away = positioned_main(wide, at_edge.x - 12.f, at_edge.y + 9.f);
    assert(away.x == at_edge.x - 12.f && away.y == at_edge.y + 9.f);
    const auto invalid = positioned_main(wide, std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity());
    assert(invalid.x == main.x && invalid.y == main.y);
    std::cout << "PASS stable main anchor, independent drawer and narrow-screen bounds\n";
    return 0;
}
}

namespace menu_background_tests
{
int run()
{
    using namespace gui::background;
    options value;
    assert(value.selected == style::classic);
    value = {static_cast<style>(-1), std::numeric_limits<float>::quiet_NaN(), INFINITY};
    normalize(value);
    assert(value.selected == style::classic && value.opacity == .85f && value.dimming == .48f);
    value = {style::custom, -1, 0};
    normalize(value);
    assert(value.selected == style::custom && value.opacity == 0 && value.dimming == .25f);
    value.opacity = 8; value.dimming = 2;
    normalize(value);
    assert(value.opacity == 1 && value.dimming == .85f);
    assert(valid_dimensions(4096, 4096));
    assert(!valid_dimensions(8192, 8192));
    assert(!valid_dimensions(8193, 1));
    assert(!valid_dimensions(0, 100));
    assert(!valid_dimensions(UINT32_MAX, UINT32_MAX));
    // Cover, not stretch: center-crop wide and portrait images symmetrically.
    auto uv = cover_uv({2000, 1000}, {1000, 1000});
    assert(uv.minimum.x == .25f && uv.maximum.x == .75f);
    assert(uv.minimum.y == 0 && uv.maximum.y == 1);
    uv = cover_uv({1000, 2000}, {1000, 1000});
    assert(uv.minimum.y == .25f && uv.maximum.y == .75f);
    assert(uv.minimum.x == 0 && uv.maximum.x == 1);
    uv = cover_uv({1000, 1000}, {1000, 1000});
    assert(uv.minimum.x == 0 && uv.minimum.y == 0 && uv.maximum.x == 1 && uv.maximum.y == 1);
    uv = cover_uv({0, 0}, {1000, 1000});
    assert(uv.minimum.x == 0 && uv.maximum.x == 1);
    return 0;
}
}

namespace menu_input_policy_tests
{
int run()
{
    using namespace services::game::menu_input;
    focused_shortcut shortcut;
    assert(!shortcut.pressed(true, true)); // first focus with a key already held
    assert(!shortcut.pressed(true, false));
    assert(shortcut.pressed(true, true));
    assert(!shortcut.pressed(true, true)); // no autorepeat
    assert(!shortcut.pressed(false, false));
    assert(!shortcut.pressed(false, true)); // presses in another app are ignored
    assert(!shortcut.pressed(true, true)); // held through Alt+Tab is not a new press
    assert(!shortcut.pressed(true, false));
    assert(shortcut.pressed(true, true));
    focused_pointer pointer;
    assert(!pointer.accepts(false, false));
    assert(!pointer.accepts(true, true)); // taskbar click held when returning
    assert(pointer.accepts(true, false));
    assert(pointer.accepts(true, true));
    assert(!pointer.accepts(false, true)); // drag loses focus
    assert(!pointer.accepts(true, true));
    assert(pointer.accepts(true, false));
    assert(!accepts_panel_pointer(true, false));
    assert(!accepts_panel_pointer(false, true));
    assert(accepts_panel_pointer(true, true));
    for (unsigned key : {0x09u, 0x1bu, 0x73u}) {
        assert(system_shortcut(key, true));
        assert(!system_shortcut(key, false));
    }
    assert(system_shortcut(0x5b, false) && system_shortcut(0x5c, false));
    assert(!system_shortcut('W', true));
    using services::game::menu_input::cursor_lease;
    cursor_lease lease;
    assert(!lease.finish(true, true, true));
    lease.begin(true);
    assert(lease.finish(true, true, true));
    assert(!lease.finish(true, true, true)); // no repeated capture on later frames
    for (int reason = 0; reason < 3; ++reason) {
        lease.begin(true);
        assert(!lease.finish(reason != 0, reason != 1, reason != 2));
        assert(!lease.finish(true, true, true)); // no late capture after alt-tab
    }
    lease.begin(false); // inventory/menu cursor was already free
    assert(!lease.finish(true, true, true));
    lease.begin(true);
    lease.invalidate(); // world/player/window changed while panel was open
    assert(!lease.finish(true, true, true));
    lease.begin(true); // fresh open/close still works after invalidation
    assert(lease.finish(true, true, true));
    std::cout << "PASS panel cursor ownership, focus, screen and session cleanup\n";
    return 0;
}
}

int main()
{
    int failures = 0;
    failures += theme_palette_tests::run();
    failures += arraylist_layout_tests::run();
    failures += hud_editor_layout_tests::run();
    failures += config_page_model_tests::run();
    failures += keybinds_presentation_tests::run();
    failures += dock_layout_tests::run();
    failures += menu_background_tests::run();
    failures += menu_input_policy_tests::run();
    return failures ? 1 : 0;
}
