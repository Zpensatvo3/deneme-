#pragma once
#include "../module_support.hpp"
#include <memory>
#include "settings/chams_settings.hpp"

namespace chams_detail { struct runtime; }
namespace modules
{
    class chams : public module
    {
    public:
        chams();
        ~chams() override;
        void run(::cache&) override;
        void render_overlay() override;
        bool render_when_disabled() const override { return true; }
        void on_enable(::cache&) override;
        void on_disable(::cache&) override;
        void render_options() override;
        float get_options_height() override { return 440.f; }
        void visit_config(config_visitor&) override;
    private:
        using settings_type = module_settings::chams;
        settings_type settings;
        std::mutex settings_mutex;
        std::atomic_bool reset_requested{true};
        std::unique_ptr<chams_detail::runtime> state;
    };
}
