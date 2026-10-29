#pragma once
#include <memory>
#include "settings/sky_settings.hpp"

namespace sky_panorama
{
    // All calls and the Fabric callback run on Minecraft's render thread.
    class renderer
    {
    public:
        renderer();
        ~renderer();
        bool connect();
        void configure(const module_settings::sky&,bool enabled,bool reduce_motion);
        void configure_colors(int sky, int fog, float strength, bool match_fog, bool enabled);
        void shutdown();
        const char* error() const;
    private:
        struct runtime;
        std::unique_ptr<runtime> state_;
    };
}
