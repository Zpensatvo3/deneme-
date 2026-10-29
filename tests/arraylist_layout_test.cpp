#include "../src/modules/render/arraylist_layout.hpp"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

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

int main()
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
