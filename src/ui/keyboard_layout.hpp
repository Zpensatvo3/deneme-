#pragma once
#include "../platform/input/key_codes.hpp"
#include <algorithm>
#include <string_view>

namespace gui::keyboard_layout
{
    struct metrics { float unit, width, height; bool scroll; };
    inline float gap(float unit) { return std::clamp(unit * .09f, 1.f, 2.5f); }
    inline metrics measure(float available)
    {
        // Preserve readable legends on small windows instead of shrinking them
        // to 7 px. Only the keyboard scrolls; the assignment list keeps its width.
        const float unit = std::max(29.f, available / 23.f);
        return {unit, unit * 23.f, unit * 6.5f, unit * 23.f > available + .5f};
    }
    constexpr const char* legend(const platform::keys::key& key)
    {
        switch (key.code) {
        case 0x08: return "Back";
        case 0x2c: return "Prt";
        case 0x91: return "Scr";
        case 0x13: return "Pau";
        case 0x24: return "Hm";
        case 0x21: return "PgU";
        case 0x22: return "PgD";
        case 0x90: return "NL";
        case 0x25: return "Lt";
        case 0x28: return "Dn";
        case 0x27: return "Rt";
        case platform::keys::num_enter: return "Ent";
        default: return key.label;
        }
    }
    constexpr bool single_character(const platform::keys::key& key)
    {
        return std::string_view(legend(key)).size() == 1;
    }
    constexpr float font_size(const platform::keys::key& key) { return single_character(key) ? 15.f : 12.5f; }
}
