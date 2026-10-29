#pragma once

namespace services::game::menu_input
{
    // Render/client thread only, including the opening/closing frame. Not auth
    // gated: the login panel must release gameplay input and the cursor too.
    void update(bool open, bool focused, bool allow_restore = true);
    void shutdown();
}
