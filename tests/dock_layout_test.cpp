#include "../src/ui/dock_layout.hpp"
#include <cassert>
#include <iostream>
#include <limits>

int main()
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
}
