#pragma once
#include <utility>

namespace services::game::menu_input
{
    inline bool accepts_panel_pointer(bool open, bool focused) { return open && focused; }

    // A key held in another app must not become a fresh menu shortcut on return.
    class focused_shortcut
    {
        bool released_ = false;
    public:
        bool pressed(bool focused, bool down)
        {
            if (!focused) { released_ = false; return false; }
            if (!down) { released_ = true; return false; }
            return std::exchange(released_, false);
        }
    };

    class focused_pointer
    {
        bool ready_ = false;
    public:
        bool accepts(bool focused, bool any_button_down)
        {
            if (!focused) ready_ = false;
            else if (!any_button_down) ready_ = true;
            return focused && ready_;
        }
    };

    inline bool system_shortcut(unsigned key, bool alt_context)
    {
        // Windows virtual-key values: Win, Alt+Tab, Alt+Escape and Alt+F4.
        return key == 0x5b || key == 0x5c ||
            (alt_context && (key == 0x09 || key == 0x1b || key == 0x73));
    }

    // Only restore capture that the panel itself took from this game session.
    class cursor_lease
    {
        bool owned_ = false;
    public:
        void begin(bool previously_locked) { owned_ = previously_locked; }
        void invalidate() { owned_ = false; }
        bool finish(bool focused, bool gameplay, bool same_session)
        {
            return std::exchange(owned_, false) && focused && gameplay && same_session;
        }
    };
}
