#include "runtime.hpp"
#include "module_loop.hpp"
#include "../cache/cache.hpp"
#include "../configs/configs.hpp"
#include "../configs/keybind_profile.hpp"
#include "../gui/gui.hpp"
#include "../loader/loader_ui.hpp"
#include "../modules/module_lifecycle.hpp"
#include "../platform/adapter/game_adapter.hpp"
#include "../platform/jni/jvm_session.hpp"
#include "../platform/test_crash_report.hpp"
#include "../platform/input/key_codes.hpp"
#include "../services/input/input_service.hpp"
#include "../services/game/hybrid_bridge.hpp"
#include "../services/game/utility_bridge.hpp"
#include "../services/game/item_bridge.hpp"
#include "../services/game/inventory_bridge.hpp"
#include "../services/game/world_bridge.hpp"
#include "../utils/version_manager.hpp"
#include <Windows.h>
#include <chrono>
#include <exception>
#include <string>
#include <thread>

namespace
{
    constexpr auto worker_interval = std::chrono::milliseconds(5);
    bool emergency_exit_pressed()
    {
        // Once input is hooked, End must mean E0 4F, not NumPad 1 with
        // NumLock off (Windows reports both as VK_END).
        return gui::get_window() ? services::input::physical_scan_down(platform::keys::scan_for(VK_END))
            : (GetAsyncKeyState(VK_END) & 0x8000) != 0;
    }

    // Declared after the game cache, so renderer/modules stop before its JNI refs die.
    struct runtime_services
    {
        cache& game;
        bool modules_started = false;
        bool renderer_started = false;
        bool configs_started = false;

        ~runtime_services()
        {
            gui::should_unload.store(true, std::memory_order_release);
            services::input::cancel_automatic_inputs();
            if (configs_started) configs::shutdown();
            loader_ui::closing();
            gui::draw = false;
            if (renderer_started) gui::shutdown();
            services::game::world_bridge::shutdown();
            services::game::inventory_bridge::shutdown();
            services::game::items::shutdown();
            services::game::utility::shutdown();
            services::game::hybrid::shutdown();
            if (modules_started) modules::shutdown(game);
        }
    };
}

void app::run()
{
    platform::test_crash_report crash_report;
    loader_ui::initialize();
    try
    {
        loader_ui::stage(0, "Locating Minecraft JVM runtime");
        while (!GetModuleHandleW(L"jvm.dll"))
        {
            if (gui::should_unload || emergency_exit_pressed()) return;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        loader_ui::stage(1, "Attaching native JVM session");
        platform::jvm_session session;
        if (!session.open()) { loader_ui::fail(session.error().c_str()); return; }

        loader_ui::stage(2, "Resolving Minecraft runtime and mappings");
        if (!mc::matches_target(mc::detect(session.env(), session.classes())))
        {
            const auto message = std::string("This DLL requires Fabric ") + std::string(mc::target.name) +
                "; detected " + mc::get_version_name() + ". Select the matching OneStep DLL.";
            loader_ui::fail(message.c_str());
            return;
        }
        platform::adapter::initialize(mc::get_version());
        const std::string version = std::string("Detected runtime: Fabric ") + mc::get_version_name();
        loader_ui::stage(2, version.c_str());

        cache game;
        runtime_services services{game};
        // Register before enabling render callbacks: the module list stays fixed
        // until gui::shutdown has joined the render side.
        loader_ui::stage(3, "Registering client modules");
        services.modules_started = true;
        modules::init();
        configs::keybind_profile::initialize();

        loader_ui::stage(4, "Installing OpenGL render hook");
        if (!gui::init()) { loader_ui::fail("OpenGL renderer hook could not be installed"); return; }
        services.renderer_started = true;
        loader_ui::ready();

        if (gui::should_unload) return;
        services.configs_started = true;
        configs::initialize();

        module_loop loop;
        while (!emergency_exit_pressed() && !gui::should_unload)
        {
            loop.tick(game);
            std::this_thread::sleep_for(worker_interval);
        }
    }
    catch (const std::exception& error) { loader_ui::fail(error.what()); }
    catch (...) { loader_ui::fail("Onestep runtime initialization or execution failed"); }
}
