#pragma once
#include "../module_support.hpp"
#include "trigger_adapter.hpp"
#include "trigger_config.hpp"

namespace modules
{
    class triggerbot : public module
    {
    public:
        triggerbot() : module("triggerbot") {}
        ~triggerbot() override { trigger_engine::request_reset(state); }
        void run(::cache&) override;
        void render_overlay() override;
        bool render_when_disabled() const override { return true; }
        void render_options() override;
        void cancel_on_client_thread();
        void on_render_shutdown() override { cancel_on_client_thread(); }
        void on_enable(::cache&) override;
        void on_disable(::cache&) override;
        float get_options_height() override { return 340.f; }
        void visit_config(config_visitor& visitor) override
        {
            if (trigger_engine::visit_config(visitor, settings)) {
                ++state.settings_revision;
                trigger_engine::request_reset(state);
            }
        }
    private:
        trigger_engine::config settings{};
        trigger_engine::adapter_runtime state{};
    };
}
