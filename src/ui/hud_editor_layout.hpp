#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace gui::hud_editor_layout
{
    struct point { float x = 0, y = 0; };
    struct rect { float x, y, width, height; };
    struct placement { point position; float guide_x = -1, guide_y = -1; };

    inline point clamp(point position, point size, point canvas, float margin = 8)
    {
        if (!std::isfinite(position.x)) position.x = margin;
        if (!std::isfinite(position.y)) position.y = margin;
        return { std::clamp(position.x, margin, std::max(margin, canvas.x - size.x - margin)),
            std::clamp(position.y, margin, std::max(margin, canvas.y - size.y - margin)) };
    }

    inline placement place(point position, point size, point canvas, bool snap, bool grid,
        const std::vector<rect>& neighbours, bool free_move = false)
    {
        placement result{ clamp(position, size, canvas) };
        if (!snap || free_move) return result;
        const point raw = result.position;
        if (grid)
        {
            result.position.x = std::round(raw.x / 8.f) * 8.f;
            result.position.y = std::round(raw.y / 8.f) * 8.f;
        }
        float best_x = 6.01f, best_y = 6.01f;
        auto consider = [&](float target, bool horizontal)
        {
            const float start = horizontal ? raw.x : raw.y;
            const float length = horizontal ? size.x : size.y;
            float& best = horizontal ? best_x : best_y;
            for (float offset : { 0.f, length * .5f, length })
            {
                const float distance = std::fabs(start + offset - target);
                if (distance <= 6.f && distance < best)
                {
                    best = distance;
                    if (horizontal) { result.position.x = target - offset; result.guide_x = target; }
                    else { result.position.y = target - offset; result.guide_y = target; }
                }
            }
        };
        for (float x : { 8.f, canvas.x * .5f, canvas.x - 8.f }) consider(x, true);
        for (float y : { 8.f, canvas.y * .5f, canvas.y - 8.f }) consider(y, false);
        for (const rect& other : neighbours)
        {
            for (float x : { other.x, other.x + other.width * .5f, other.x + other.width }) consider(x, true);
            for (float y : { other.y, other.y + other.height * .5f, other.y + other.height }) consider(y, false);
        }
        const point bounded = clamp(result.position, size, canvas);
        if (bounded.x != result.position.x) result.guide_x = -1;
        if (bounded.y != result.position.y) result.guide_y = -1;
        result.position = bounded;
        return result;
    }
}
