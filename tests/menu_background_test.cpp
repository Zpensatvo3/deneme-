#include "../src/ui/menu_background.hpp"
#include <cassert>
#include <limits>

int main()
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
}
