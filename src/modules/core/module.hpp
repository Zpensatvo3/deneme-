#pragma once

#include "../../utils/utils.hpp"
#include <cstdint>
#include <atomic>
#include <random>
#include <string>
#include "manager.hpp"

class cache;

namespace modules
{
    inline constexpr std::uintptr_t synthetic_mouse_tag = 0x574F4C46434C4943ULL;

    // Shared module contract. Gameplay modules only implement behaviour; the
    // registry, UI and config layers use this stable surface.
    class module
    {
    public:
        // Command property, not native state: writes are Java manager requests.
        class enabled_request
        {
        public:
            explicit enabled_request(module* owner) : owner_(owner) {}
            operator bool() const { const auto state = manager::find(owner_->get_id()); return state && state->enabled; }
            enabled_request& operator=(bool value) { manager::enable(owner_->get_id(), value); return *this; }
            enabled_request& operator=(const enabled_request& other) { return *this = static_cast<bool>(other); }
        private:
            module* const owner_;
        };
        module(const char* name = "no name", const char* category = "Combat", const char* id = nullptr);
        virtual ~module() = default;

        virtual void run(::cache& cache);
        virtual void render_options();
        virtual void render_overlay() {};
        void run_managed(::cache& context) { manager::view_scope view; run(context); }
        void render_managed_overlay() { manager::view_scope view; render_overlay(); }
        void render_managed_options() { manager::view_scope view; render_options(); }
        virtual bool render_when_disabled() const { return false; }
        // Render-thread cleanup before the swap hook and client JNI go away.
        virtual void on_render_shutdown() {}
        virtual void on_enable(::cache& cache);
        virtual void on_disable(::cache& cache);

        int get_keybind() const;
        manager::result set_keybind(int value);
        virtual bool supports_keybind() const { const auto state = manager::find(get_id()); return state && state->supports_keybind; }
        const char* get_id() const;
        const char* get_name() const;
        const char* get_category() const;
        std::uint64_t get_generation() const { const auto state = manager::find(get_id()); return state ? state->generation : 0; }
        bool lifecycle_active() const { return prev_enabled; }
        void key_bind_selector(float width = 132.f);
        void begin_keybind_capture();
        bool is_keybind_capture_active() const { return scanning; }

        bool display_options = false;
        enabled_request enabled{this};
        virtual float get_options_height() { return 45.0f; }
        // Legacy height describes custom controls; informational-only panels
        // explicitly opt out without changing their renderer or gameplay.
        virtual bool has_custom_options() { return get_options_height() > 45.0f; }

    protected:
        std::random_device rd{};
        std::mt19937_64 gen{ rd() };

    private:
        template<class Entry, class Context> friend void synchronize_lifecycle(Entry&, Context&);
        template<class Entry, class Context> friend void stop_module(Entry&, Context&);
        // Worker-thread observations only, not another requested-state owner.
        bool prev_enabled = false;
        std::uint64_t prev_generation = 0;
        utils::null_string keycode_to_string(int keycode) const;
        static std::string make_stable_id(const char* name);

        bool scanning = false;
        std::string id_storage;
        std::string name = "no name";
        std::string category = "Combat";
    };
}
