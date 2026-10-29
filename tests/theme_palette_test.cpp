#include "../src/gui/theme_palette.hpp"
#include <iostream>
#include <limits>

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
int main()
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
}
