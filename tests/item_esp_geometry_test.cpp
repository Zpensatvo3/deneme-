#include "../src/modules/visual/item_esp_geometry.hpp"
#include <cassert>
#include <iostream>

int main()
{
    using namespace item_esp_geometry;
    camera c{{0, 0, 0}, 0, 0, 70, 1920, 1080, 100, 50};
    rectangle box;
    assert(project_box(c, {-.125, -.125, 5}, {.125, .125, 5.25}, box));
    assert(box.left < 1060 && box.right > 1060 && box.top < 590 && box.bottom > 590);
    assert(!project_box(c, {-.125, -.125, -5}, {.125, .125, -4.75}, box));
    assert(!project_box(c, {100, 0, 5}, {101, 1, 6}, box));
    assert(project_box(c, {-.125, -.125, -.1}, {.125, .125, .2}, box));
    assert(std::isfinite(box.left) && box.left >= c.x && box.right <= c.x + c.width);
    for (const double distance : {8., 32., 96., 256., 1024., 30000000.})
    {
        assert(project_box(c, {-.125, -.125, distance}, {.125, .125, distance + .25}, box));
        assert(box.right - box.left >= 3 && box.bottom - box.top >= 3);
    }
    c.yaw = 90;
    assert(project_box(c, {-5.25, -.125, -.125}, {-5, .125, .125}, box));
    c.pitch = 90; c.yaw = 0;
    assert(project_box(c, {-.125, -5.25, -.125}, {.125, -5, .125}, box));
    c.fov = 0;
    assert(!project_box(c, {0, 0, 5}, {1, 1, 6}, box));
    c.fov = 70; c.width = std::numeric_limits<double>::quiet_NaN();
    assert(!project_box(c, {0, 0, 5}, {1, 1, 6}, box));
    c.width = 1920;
    assert(!project_box(c, {1, 1, 5}, {0, 0, 6}, box));
    assert(!project_box(c, {0, 0, std::numeric_limits<double>::infinity()}, {1, 1, 6}, box));
    assert(label("Diamond", 12, 256, true, true, true) == "Diamond  x12  256.0m");
    assert(label("Diamond", 1, 12, true, false, false) == "Diamond");
    assert(label("Diamond", 12, 12, false, true, false) == "x12");
    assert(label("Diamond", 12, 12, false, false, true) == "12.0m");
    assert(label("Diamond", 12, 12, false, false, false).empty());
    assert(label("", 0, -1, false, true, true).empty());
    std::cout << "Item ESP: projection, near-plane clipping, distant loaded items and labels passed\n";
}
