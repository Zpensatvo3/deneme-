#pragma once

namespace services::game::items
{
    // Called on the client/render thread, including disabled frames for reset.
    void update(bool enabled, int min_cps, int max_cps);
    void shutdown();
}
