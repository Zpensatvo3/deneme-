#include "../src/ui/hud_editor_layout.hpp"
#include <iostream>
#include <limits>

int main()
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
}
