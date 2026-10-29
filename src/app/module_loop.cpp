#include "module_loop.hpp"
#include "../cache/cache.hpp"
#include "../gui/gui.hpp"
#include "../modules/core/module_registry.hpp"
#include "../platform/jni/jni_context.hpp"
#include "../services/input/input_service.hpp"
#include "../platform/input/key_codes.hpp"
#include <Windows.h>

namespace
{
    bool gameplay_input_allowed(const cache& game)
    {
        const HWND window = gui::get_window();
        if (gui::draw || !gui::runtime_ready.load(std::memory_order_acquire) || !game.is_valid() || !window ||
            GetForegroundWindow() != window) return false;
        try
        {
            auto screen = game.instance.currentScreen.get();
            return !platform::jni_context::clear_exception() && !screen;
        }
        catch (...)
        {
            platform::jni_context::clear_exception();
            return false;
        }
    }
}

void app::module_loop::tick(::cache& game)
{
    game.update();
    const bool allow_input = gameplay_input_allowed(game);
    for (modules::module* entry : modules::get_modules())
    {
        if (gui::should_unload) break;
        const int binding = entry->supports_keybind() ? entry->get_keybind() : 0;
        bool pressed = binding > 0 && binding <= 255 && binding != gui::menu_key &&
            (GetAsyncKeyState(binding) & 0x8000) != 0;
        if (binding == VK_SPACE)
            pressed = binding != gui::menu_key && services::input::physical_key_down(VK_SPACE);
        else if (const auto scan = platform::keys::scan_for(binding))
            pressed = services::input::physical_scan_down(scan);
        if (keys_[entry].sample(binding, pressed, allow_input))
            entry->enabled = !entry->enabled;
        modules::tick_module(*entry, game);
    }
}
