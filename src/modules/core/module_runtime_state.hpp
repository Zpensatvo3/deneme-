#pragma once

namespace modules
{
    // Numeric input state, independent of Windows/JNI/UI for offline testing.
    class key_edge
    {
    public:
        bool sample(int binding, bool pressed, bool allowed)
        {
            if (binding != binding_)
            {
                const bool rebound = initialized_;
                binding_ = binding;
                initialized_ = true;
                if (rebound) { pressed_ = pressed; return false; }
            }
            const bool rising = pressed && !pressed_;
            pressed_ = pressed;
            return binding > 0 && allowed && rising;
        }
    private:
        int binding_ = 0;
        bool initialized_ = false;
        bool pressed_ = false;
    };

    template<class Module, class Context>
    void synchronize_lifecycle(Module& entry, Context& context)
    {
        const bool requested = entry.enabled;
        if (requested == entry.prev_enabled) return;
        entry.prev_enabled = requested;
        if (requested) entry.on_enable(context);
        else entry.on_disable(context);
    }

    template<class Module, class Context>
    void stop_module(Module& entry, Context& context)
    {
        if (!entry.enabled && !entry.prev_enabled) return;
        entry.enabled = false;
        entry.prev_enabled = false;
        entry.on_disable(context);
    }

    template<class Module, class Context>
    void tick_module(Module& entry, Context& context)
    {
        synchronize_lifecycle(entry, context);
        if (entry.enabled) entry.run(context);
        // One-shot modules may disable themselves from run/on_enable.
        synchronize_lifecycle(entry, context);
    }
}
