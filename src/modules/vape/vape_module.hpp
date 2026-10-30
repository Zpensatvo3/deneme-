#pragma once
#include "../module_support.hpp"
#include "vape_config.hpp"
#include "../core/settings_store.hpp"
#include <memory>

namespace vape_bridge { struct runtime; }
namespace modules
{
    // The bridge owns IO/lifecycle only. Decisions stay in the supplied headers.
    class vape_module : public module
    {
    public:
        explicit vape_module(vape_kind kind);
        ~vape_module() override;
        void run(::cache&) override {}
        void on_enable(::cache&) override;
        void on_disable(::cache&) override;
        void render_overlay() override;
        bool render_when_disabled() const override { return true; }
        void render_options() override;
        float get_options_height() override;
    private:
        using options = vape_settings::config;
        settings::store<options> settings;
        const vape_kind kind;
        std::atomic_bool reset_requested{true};
        std::unique_ptr<vape_bridge::runtime> state;
        std::string runtime_issue;
        const char* conflict() const;
    };

    module* create_vape_aim();
}
