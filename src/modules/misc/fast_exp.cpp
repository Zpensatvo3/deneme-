#include <Windows.h>
#include "../../cache/cache.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../services/input/input_service.hpp"
#include "../../services/game/item_bridge.hpp"
#include "../../services/game/overlay_context.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>

#include <imgui/imgui.h>

#include "fast_exp.hpp"
#include "../../ui/settings_renderer.hpp"
#include "../ui_redirect.hpp"
#include "../../gui/gui.hpp"
#include "../../mappings.hpp"

namespace
{
	using steady_clock = std::chrono::steady_clock;

	bool stack_is_exp(const maps::ItemStack& stack, const maps::Item& exp)
	{
		if (!stack || !exp || stack.isEmpty.call() == JNI_TRUE || platform::jni_context::clear_exception())
			return false;
		return stack.isOf.call(exp) == JNI_TRUE && !platform::jni_context::clear_exception();
	}
}

struct modules::fast_exp::runtime
{
    services::game::overlay_session session;
    bool using_java = false;
};

modules::fast_exp::fast_exp() : module("fast exp", "World"), runtime_(std::make_unique<runtime>())
{
    active_instance.store(this);
}
modules::fast_exp::~fast_exp()
{
    fast_exp* expected = this;
    active_instance.compare_exchange_strong(expected, nullptr);
}

void modules::fast_exp::notify_manual_right_click(bool pressed)
{
	fast_exp* instance = active_instance.load();
	if (instance)
	{
		instance->manual_right_down.store(pressed);
	}
}

void modules::fast_exp::schedule_next_click(steady_clock::time_point now)
{
    auto settings_snapshot = settings_.read();
	const int low = std::clamp((std::min)(settings_snapshot.min_cps, settings_snapshot.max_cps), 1, 50);
	const int high = std::clamp((std::max)(settings_snapshot.min_cps, settings_snapshot.max_cps), low, 50);
	std::uniform_int_distribution<int> cps_distribution(low, high);
	const double selected_cps = static_cast<double>(cps_distribution(gen));

	// Convert CPS into milliseconds interval
	const double base_interval_ms = 1000.0 / (std::max)(1.0, selected_cps);

	// Keep the existing +/- 1.5ms timing jitter; this is not a server-acceptance guarantee.
	std::uniform_real_distribution<double> jitter_distribution(-1.5, 1.5);
	const int interval_ms = (std::max)(10, static_cast<int>(std::lround(base_interval_ms + jitter_distribution(gen))));

	next_click = now + std::chrono::milliseconds(interval_ms);
}

void modules::fast_exp::stop()
{
	was_held = false;
	next_click = {};
	platform::jni_context::clear_exception();
	// No owned DOWN survives a pulse. Never release the user's right hold when
	// EXP is absent: doing so also interrupts potions and Fast Place.
}

void modules::fast_exp::run(::cache&)
{
	// Item use execution runs from render_overlay() on Minecraft's client/render thread.
}

void modules::fast_exp::render_overlay()
{
    if constexpr (mc::target.build_id == 12111)
    {
        if (!platform::jni_context::bind_current_thread())
        {
            reset_requested_.store(true, std::memory_order_release);
            return;
        }
        if (reset_requested_.exchange(false, std::memory_order_acq_rel) || !enabled)
        {
            blocked_until_release = false;
            runtime_->session.reset();
            stop();
            if (!enabled) return;
        }
    }
	const HWND minecraft_window = gui::get_window();
    const bool held = mc::target.build_id == 12111
        ? services::input::physical_button_down(services::input::mouse_button::right)
        : manual_right_down.load() || (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;

	// Safety check: Physical right-click must be held
	if (!held)
	{
		manual_right_down.store(false);
		blocked_until_release = false;
        if constexpr (mc::target.build_id == 12111) runtime_->session.reset();
		stop();
		return;
	}

	// Safety check: Don't click if Wolf UI/Loader is active or game window is not in foreground
	if (gui::draw || gui::loader_visible || gui::should_unload || !minecraft_window ||
		GetForegroundWindow() != minecraft_window || !platform::jni_context::bind_current_thread())
	{
		stop();
        if constexpr (mc::target.build_id == 12111) runtime_->session.reset();
		return;
	}

	// Safety check: If previously blocked (e.g., ran out of EXP), wait until user releases the button
	if (blocked_until_release)
		return;

	try
	{
		jni::frame frame{ 64 };
        if constexpr (mc::target.build_id == 12111)
        {
            services::game::items::exp_snapshot observed;
            const auto backend = services::game::items::capture_exp(observed);
            const bool using_java = backend != services::game::hybrid::result::native_fallback;
            if (runtime_->using_java != using_java) { runtime_->session.reset(); stop(); }
            runtime_->using_java = using_java;
            if (using_java)
            {
                if (backend != services::game::hybrid::result::ready)
                { runtime_->session.reset(); stop(); return; }
                const maps::ClientWorld world{observed.world, false};
                const maps::ClientPlayerEntity player{observed.player, false};
                if (runtime_->session.observe({world, player})) stop();
                if (!observed.holding_exp) { blocked_until_release = true; stop(); return; }
                const auto now = steady_clock::now();
                if (!was_held) { was_held = true; next_click = now; }
                if (now < next_click) return;
                if (!enabled || !services::game::items::use_exp(observed))
                {
                    // A mutator may have executed before an error. Do not retry
                    // through either backend until the physical hold is released.
                    blocked_until_release = true;
                    stop();
                    return;
                }
                schedule_next_click(now);
                return;
            }
        }
		maps::MinecraftClient instance = maps::MinecraftClient{}.instance.get();
		maps::ClientPlayerEntity player = instance ? instance.player.get() : maps::ClientPlayerEntity{};
		if (!instance || !player || platform::jni_context::clear_exception())
		{
			stop();
			return;
		}

		// Safety check: Don't click when any in-game screen (chat, inventory, container, pause menu) is open
		if (instance.currentScreen.get() || platform::jni_context::clear_exception())
		{
			blocked_until_release = false;
			stop();
			return;
		}

		// Safety check: Ensure the player is actively holding an Experience Bottle in main hand or off hand
		maps::Item exp_item = maps::Items{}.EXPERIENCE_BOTTLE.get();
		if (!exp_item || platform::jni_context::clear_exception())
		{
			blocked_until_release = true;
			stop();
			return;
		}

		maps::ItemStack main_hand = player.getMainHandStack.call();
		maps::ItemStack off_hand = player.getOffHandStack.call();
		if (platform::jni_context::clear_exception())
		{
			blocked_until_release = true;
			stop();
			return;
		}

		const bool holding_exp = stack_is_exp(main_hand, exp_item) ||
			stack_is_exp(off_hand, exp_item);
		if (!holding_exp)
		{
			// Prevent accidental usage of other items once EXP runs out
			blocked_until_release = true;
			stop();
			return;
		}

		const auto now = steady_clock::now();
		if (!was_held)
		{
			was_held = true;
			next_click = now;
		}

		if (now < next_click)
			return;

		// Clear vanilla 4-tick right-click delay so SendInput clicks process without throttling
		instance.itemUseCooldown.set(0);
		platform::jni_context::clear_exception();

        // Keep older targets unchanged. The 1.21.11 fallback also uses only one
        // vanilla entry point, so a rejected Java binding cannot reintroduce duplicates.
        if constexpr (mc::target.build_id != 12111)
            services::input::mouse_click(services::input::mouse_button::right, nullptr, true);

		// Invoke vanilla item use on the client thread; server acceptance is separate.
		instance.doItemUse.call();
		platform::jni_context::clear_exception();

		schedule_next_click(now);
	}
	catch (...)
	{
		platform::jni_context::clear_exception();
		manual_right_down.store(false);
        blocked_until_release = mc::target.build_id == 12111;
		stop();
	}
}

void modules::fast_exp::on_disable(::cache& cache)
{
	manual_right_down.store(false);
    if constexpr (mc::target.build_id == 12111)
    {
        reset_requested_.store(true, std::memory_order_release);
        return; // Session/timer state is owned by the render thread.
    }
	blocked_until_release = false;
	stop();
}

void modules::fast_exp::on_render_shutdown()
{
    if constexpr (mc::target.build_id == 12111)
    {
        runtime_->session.reset();
        blocked_until_release = false;
        stop();
    }
}

void modules::fast_exp::render_options()
{
    gui::settings::render(settings_, module_settings::fast_exp::schema());
}
