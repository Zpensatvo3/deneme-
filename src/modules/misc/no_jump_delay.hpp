#pragma once

#include "../module_support.hpp"

namespace modules
{
    class no_jump_delay final : public module
    {
    public:
        no_jump_delay() : module("no jump delay", "Movement", "no_jump_delay") {}

        // Game-state writes belong to the existing client-frame callback, not
        // the background module worker. There is no cached world or input to own.
        void run(::cache&) override {}
        void render_overlay() override;
        void render_options() override;
        float get_options_height() override { return 90.0f; }
        bool has_custom_options() override { return false; }
    };
}
