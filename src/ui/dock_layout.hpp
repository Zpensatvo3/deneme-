#pragma once
#include <algorithm>
#include <cmath>

namespace gui::dock_layout
{
    struct box { float x, y, width, height; };
    constexpr float margin = 8.f, gap = 16.f, settings_width = 304.f;
    inline box main_panel(box work, float offset_x = 0.f, float offset_y = 0.f)
    {
        const float width = std::min(960.f, std::max(1.f, work.width - margin * 2));
        const float height = std::min(620.f, std::max(1.f, work.height - margin * 2));
        return {std::clamp(work.x + (work.width - width) * .5f + offset_x, work.x + margin,
                    std::max(work.x + margin, work.x + work.width - margin - width)),
                std::clamp(work.y + (work.height - height) * .5f + offset_y, work.y + margin,
                    std::max(work.y + margin, work.y + work.height - margin - height)), width, height};
    }
    inline box settings_panel(box work, box main)
    {
        const float width = std::min(settings_width, std::max(1.f, work.width - margin * 2));
        // A small display gets a right-side overlay, never a shifted/resized main panel.
        return {std::clamp(main.x + main.width + gap, work.x + margin,
                    std::max(work.x + margin, work.x + work.width - margin - width)), main.y, width, main.height};
    }
    inline box positioned_main(box work, float x, float y)
    {
        auto panel = main_panel(work);
        if (std::isfinite(x)) panel.x = std::clamp(x, work.x + margin,
            std::max(work.x + margin, work.x + work.width - margin - panel.width));
        if (std::isfinite(y)) panel.y = std::clamp(y, work.y + margin,
            std::max(work.y + margin, work.y + work.height - margin - panel.height));
        return panel;
    }
    inline bool preview_fits(box work, box main, float width)
    {
        return main.x - work.x - margin >= width + gap;
    }
}
