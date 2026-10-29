#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

// Pure ArrayList geometry. This layer deliberately stays free of ImGui, the GUI
// globals and the game SDK so the overlay and the HUD editor share one source of
// truth and so the layout rules stay unit testable on their own.
namespace modules::arraylist_layout
{
    struct geometry
    {
        float font_size = 0.0f;   // real drawing point size
        float padding_x = 0.0f;
        float padding_y = 0.0f;
        float bar_width = 0.0f;
        float keybind_gap = 0.0f;  // gap between the label and its keybind chip
        float chip_padding = 0.0f; // horizontal padding inside the chip
        float row_gap = 0.0f;
        float keybind_font_size = 0.0f;

        float line_height() const { return font_size + padding_y * 2.0f; }
    };

    struct row
    {
        std::string key;      // stable module id, animation cache key
        std::string label;    // display name
        std::string keybind;  // empty means nothing is drawn
        float visibility = 0.0f;
        float text_width = 0.0f;
        float keybind_width = 0.0f;
    };

    struct result
    {
        std::vector<float> row_top;  // offset from the block top, in draw order
        std::vector<float> row_left; // distance from the right edge to this row's left edge
        float width = 0.0f;
        float height = 0.0f;
        bool drawable = false;
    };

    // Longest label on top; equally wide labels fall back to name order. Rows keep
    // their relative order otherwise, matching the original stable_sort behaviour.
    inline void sort_rows(std::vector<row>& rows)
    {
        std::stable_sort(rows.begin(), rows.end(), [](const row& left, const row& right)
        {
            if (std::fabs(left.text_width - right.text_width) < 0.01f)
                return left.label < right.label;
            return left.text_width > right.text_width;
        });
    }

    inline float row_width(const row& item, const geometry& metrics)
    {
        float width = item.text_width + metrics.padding_x * 2.0f + metrics.bar_width;
        if (!item.keybind.empty())
            width += metrics.keybind_gap + item.keybind_width + metrics.chip_padding * 2.0f;
        return width;
    }

    // Badge width is part of the visible row, so it also participates in sorting.
    inline void sort_rows(std::vector<row>& rows, const geometry& metrics)
    {
        std::stable_sort(rows.begin(), rows.end(), [&metrics](const row& left, const row& right)
        {
            const float left_width = row_width(left, metrics);
            const float right_width = row_width(right, metrics);
            if (std::fabs(left_width - right_width) < 0.01f)
                return left.label < right.label;
            return left_width > right_width;
        });
    }

    // Expects already sorted rows. Returns per-row offsets in that same order so
    // every draw pass reuses one geometry instead of recomputing rectangles.
    inline result build(const std::vector<row>& rows, const geometry& metrics)
    {
        result output;
        output.row_top.reserve(rows.size());
        output.row_left.reserve(rows.size());

        const float line_height = metrics.line_height();
        float widest = 0.0f;
        float accumulated = 0.0f;

        for (std::size_t index = 0; index < rows.size(); ++index)
        {
            const row& item = rows[index];
            const float width = row_width(item, metrics);
            if (width > widest) widest = width;
            output.row_top.push_back(accumulated);
            const float visibility = std::clamp(item.visibility, 0.0f, 1.0f);
            const float gap = index + 1 < rows.size()
                ? metrics.row_gap * std::min(visibility, std::clamp(rows[index + 1].visibility, 0.0f, 1.0f))
                : 0.0f;
            accumulated += line_height * visibility + gap;
        }

        if (rows.empty() || accumulated <= 0.5f) return output;

        output.height = accumulated;
        output.width = widest;
        output.drawable = true;

        // row_left is measured from the right edge, so the renderer can place every row
        // as right_edge - row_left without knowing the row width again. Wider rows
        // therefore have a larger value and start further left.
        for (std::size_t index = 0; index < rows.size(); ++index)
            output.row_left.push_back(row_width(rows[index], metrics));

        return output;
    }

    // Keeps the whole block on screen while leaving an in-bounds anchor untouched.
    // Very small screens settle for a non-negative margin instead of looping.
    inline void clamp_anchor(float& right_margin, float& top, float width, float height,
        float screen_width, float screen_height, float margin)
    {
        right_margin = std::max(0.0f, right_margin);
        top = std::max(0.0f, top);

        if (width <= 0.0f || height <= 0.0f) return;
        if (screen_width <= 0.0f || screen_height <= 0.0f) return;

        const float max_right = screen_width - width - margin;
        const float max_top = screen_height - height - margin;

        // The block cannot fit next to a margin: keep it flush and let the editor
        // report the true bounds instead of clamping into an invisible corner.
        right_margin = max_right > margin
            ? std::clamp(right_margin, margin, max_right)
            : margin;
        top = max_top > margin
            ? std::clamp(top, margin, max_top)
            : margin;
    }
}
