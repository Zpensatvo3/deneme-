#pragma once
#include <cstdint>

namespace services::input
{
    inline constexpr std::uint32_t jump_key_prefix = 0xd5000000u;
    inline bool jump_key_tag(std::uintptr_t tag) { return (static_cast<std::uint32_t>(tag) & 0xff000000u) == jump_key_prefix; }
    inline std::uintptr_t jump_key_event_tag(std::uint32_t id, bool pressed) { return jump_key_prefix | (id << 1) | (pressed ? 1u : 0u); }
    struct jump_key_ledger
    {
        std::uint32_t next = 0, active = 0;
        bool cancelled = true, delivered = false, up_queued = true;
        std::uint32_t reserve()
        {
            if (((!cancelled || !up_queued) && active) || next == 0x7fffffu) return 0;
            active = ++next; cancelled = false; delivered = false; up_queued = false; return active;
        }
        void cancel(std::uint32_t id) { if (id && id == active) cancelled = true; }
        bool observe(std::uintptr_t tag, bool pressed, bool physical_down, bool valid = true)
        {
            const auto encoded = static_cast<std::uint32_t>(tag);
            const auto id = (encoded & 0x00ffffffu) >> 1;
            if (!jump_key_tag(tag) || !id || id != active || (encoded & 1u) != static_cast<unsigned>(pressed)) return false;
            if (cancelled) return !pressed && !physical_down;
            if (pressed && (!valid || physical_down)) { cancel(id); return false; }
            if (pressed) { if (delivered) return false; delivered = true; }
            return true;
        }
    };
    // GLFW symbolic key codes -> Windows virtual keys. Unsupported keys fail
    // closed; scancode bindings are handled separately using recorded scancodes.
    inline unsigned windows_key_for_glfw(int key)
    {
        if ((key >= 48 && key <= 57) || (key >= 65 && key <= 90) || key == 32) return static_cast<unsigned>(key);
        if (key >= 290 && key <= 313) return static_cast<unsigned>(0x70 + key - 290);
        if (key >= 320 && key <= 329) return static_cast<unsigned>(0x60 + key - 320);
        switch (key) {
        case 256: return 0x1b; case 257: return 0x0d; case 258: return 0x09; case 259: return 0x08;
        case 260: return 0x2d; case 261: return 0x2e; case 262: return 0x27; case 263: return 0x25;
        case 264: return 0x28; case 265: return 0x26; case 266: return 0x21; case 267: return 0x22;
        case 268: return 0x24; case 269: return 0x23; case 280: return 0x14; case 281: return 0x91;
        case 282: return 0x90; case 283: return 0x2c; case 284: return 0x13;
        case 330: return 0x6e; case 331: return 0x6f; case 332: return 0x6a; case 333: return 0x6d;
        case 334: return 0x6b; case 335: return 0x0d;
        case 340: return 0xa0; case 341: return 0xa2; case 342: return 0xa4; case 343: return 0x5b;
        case 344: return 0xa1; case 345: return 0xa3; case 346: return 0xa5; case 347: return 0x5c;
        case 39: return 0xde; case 44: return 0xbc; case 45: return 0xbd; case 46: return 0xbe;
        case 47: return 0xbf; case 59: return 0xba; case 61: return 0xbb; case 91: return 0xdb;
        case 92: return 0xdc; case 93: return 0xdd; case 96: return 0xc0;
        default: return 0;
        }
    }
}
