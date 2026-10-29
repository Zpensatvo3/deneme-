#include "inventory_bridge.hpp"
#include "../../gui/gui.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../input/input_service.hpp"
#include "../../sdk/minecraft/common/client.hpp"
#include "../../../resource.h"
#include <Windows.h>
#include <cmath>
#include <stdexcept>
#include <limits>

namespace
{
    jobject bridge = nullptr;
    jmethodID prepare_method = nullptr, cancel_method = nullptr, stop_method = nullptr;
    jmethodID container_capture_method = nullptr, container_act_method = nullptr, mouse_tick_method = nullptr;
    bool setup_failed = false;
    std::array<bool, 6> prepared{}, rejected{}, suspended{};
    bool permitted()
    {
        const HWND window = gui::get_window();
        return !gui::draw && gui::runtime_ready.load(std::memory_order_acquire) && !gui::should_unload &&
            window && GetForegroundWindow() == window;
    }
    template<class T> T checked(JNIEnv* env, T value)
    {
        if (!value || env->ExceptionCheck()) throw std::runtime_error("Inventory JNI bridge");
        return value;
    }
    bool connect(JNIEnv* env)
    {
        if (setup_failed) return false;
        if (bridge) return true;
        try {
            auto game = checked(env, jni::get_cached_jclass<maps::MinecraftClient>());
            auto klass = checked(env, env->FindClass("java/lang/Class"));
            auto loader_method = checked(env, env->GetMethodID(klass, "getClassLoader", "()Ljava/lang/ClassLoader;"));
            auto parent = checked(env, env->CallObjectMethod(game, loader_method));
            auto loader_type = checked(env, env->FindClass("java/net/URLClassLoader"));
            auto loader_constructor = checked(env, env->GetMethodID(loader_type, "<init>", "([Ljava/net/URL;Ljava/lang/ClassLoader;)V"));
            auto url = checked(env, env->FindClass("java/net/URL"));
            auto urls = checked(env, env->NewObjectArray(0, url, nullptr));
            auto loader = checked(env, env->NewObject(loader_type, loader_constructor, urls, parent));
            HMODULE owner = nullptr;
            if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(&connect), &owner)) throw std::runtime_error("Inventory resource owner");
            auto resource = FindResourceW(owner, MAKEINTRESOURCEW(IDR_INVENTORY_BRIDGE), RT_RCDATA);
            auto loaded = resource ? LoadResource(owner, resource) : nullptr;
            auto bytes = loaded ? LockResource(loaded) : nullptr;
            const auto size = resource ? SizeofResource(owner, resource) : 0;
            if (!bytes || !size || size > 0x7fffffffU) throw std::runtime_error("Inventory resource");
            auto type = checked(env, env->DefineClass("net/onestep/hybrid/InventoryBridge", loader,
                static_cast<const jbyte*>(bytes), static_cast<jsize>(size)));
            auto constructor = checked(env, env->GetMethodID(type, "<init>", "(Ljava/lang/ClassLoader;I)V"));
            prepare_method = checked(env, env->GetMethodID(type, "prepare", "(I)V"));
            container_capture_method = checked(env, env->GetMethodID(type, "captureContainer", "(I)[Ljava/lang/Object;"));
            container_act_method = checked(env, env->GetMethodID(type, "containerAct", "([Ljava/lang/Object;IIIZZZ)Z"));
            mouse_tick_method = checked(env, env->GetMethodID(type, "tickMouse", "(ZZZZ)V"));
            cancel_method = checked(env, env->GetMethodID(type, "cancel", "(I)V"));
            stop_method = checked(env, env->GetMethodID(type, "stop", "()V"));
            auto instance = checked(env, env->NewObject(type, constructor, parent, static_cast<jint>(mc::target.build_id)));
            bridge = checked(env, env->NewGlobalRef(instance));
            return true;
        } catch (...) {
            platform::jni_context::clear_exception(); setup_failed = true;
            OutputDebugStringW(L"[OneStep] Inventory bridge setup unavailable; existing backends retained.\n");
            return false;
        }
    }
}

services::game::hybrid::result services::game::inventory_bridge::capture_container(feature operation, container_snapshot& out)
{
    out = {};
    if constexpr (mc::target.build_id != 12111) return hybrid::result::native_fallback;
    const auto f = static_cast<unsigned>(operation);
    if (f != 4 || !permitted() || !platform::jni_context::bind_current_thread()) return hybrid::result::unavailable;
    if (suspended[f]) return hybrid::result::unavailable;
    if (rejected[f] || !connect(jni::get_env())) return hybrid::result::native_fallback;
    auto env = jni::get_env();
    if (!prepared[f]) {
        env->CallVoidMethod(bridge, prepare_method, static_cast<jint>(f));
        if (platform::jni_context::clear_exception()) { rejected[f] = true; return hybrid::result::native_fallback; }
        prepared[f] = true;
    }
    try {
        auto packet = static_cast<jobjectArray>(env->CallObjectMethod(bridge, container_capture_method, static_cast<jint>(f)));
        if (env->ExceptionCheck()) throw std::runtime_error("Container capture");
        if (!packet) return hybrid::result::unavailable;
        if (env->GetArrayLength(packet) != 11) throw std::runtime_error("Container packet");
        auto owner = checked(env, env->GetObjectArrayElement(packet, 0));
        if (!env->IsSameObject(owner, bridge)) throw std::runtime_error("Container owner");
        auto raw = checked(env, env->GetObjectArrayElement(packet, 10));
        auto double_type = checked(env, env->FindClass("[D"));
        if (!env->IsInstanceOf(raw, double_type)) throw std::runtime_error("Container numeric type");
        auto data = static_cast<jdoubleArray>(raw);
        const int length = env->GetArrayLength(data);
        if (length < 19 || length > 10 + 9 * 4096 || (length - 10) % 9) throw std::runtime_error("Container shape");
        std::vector<double> values(length);
        env->GetDoubleArrayRegion(data, 0, length, values.data());
        if (env->ExceptionCheck()) throw std::runtime_error("Container numeric read");
        for (double value : values) if (!std::isfinite(value) || std::floor(value) != value) throw std::runtime_error("Container number");
        const int count = (length - 10) / 9;
        const auto boolean = [](double v) { return v == 0 || v == 1; };
        const auto integer = [](double v) { return v >= 0 && v <= (std::numeric_limits<int>::max)(); };
        if (values[0] != 1 || values[1] != f || values[2] <= 0 || !integer(values[3]) ||
            values[4] < -1 || values[4] >= count || !boolean(values[5]) || !integer(values[6]) ||
            !integer(values[7]) || values[8] != count || !boolean(values[9])) throw std::runtime_error("Container schema");
        out.operation = operation; out.sync_id = static_cast<int>(values[3]); out.hover = static_cast<int>(values[4]);
        out.cursor_empty = values[5] == 1; out.cursor_count = static_cast<int>(values[6]);
        out.cursor_limit = static_cast<int>(values[7]); out.confirmed = values[9] == 1;
        for (int i = 0; i < count; ++i) {
            const auto* v = values.data() + 10 + i * 9;
            if (v[0] != i || v[1] < -1 || v[1] > (std::numeric_limits<int>::max)() || !integer(v[2]) ||
                !boolean(v[3]) || !boolean(v[4]) || !boolean(v[5]) || !boolean(v[6]) || !integer(v[7]) || !boolean(v[8]))
                throw std::runtime_error("Container slot");
            out.slots.push_back({i, static_cast<int>(v[1]), static_cast<int>(v[2]), static_cast<int>(v[7]),
                v[3] == 1, v[4] == 1, v[5] == 1, v[6] == 1, v[8] == 1});
        }
        out.player = checked(env, env->GetObjectArrayElement(packet, 1));
        out.world = checked(env, env->GetObjectArrayElement(packet, 2));
        out.screen = checked(env, env->GetObjectArrayElement(packet, 3));
        out.handler = checked(env, env->GetObjectArrayElement(packet, 4));
        out.packet = packet;
        return hybrid::result::ready;
    } catch (...) {
        platform::jni_context::clear_exception(); suspended[f] = true; cancel(operation); out = {};
        OutputDebugStringW(L"[OneStep] Container capability suspended; no inventory replay.\n");
        return hybrid::result::unavailable;
    }
}

bool services::game::inventory_bridge::container_act(const container_snapshot& observed, int action, int source, int target)
{
    if (!bridge || !observed.packet || !permitted()) return false;
    const auto f = static_cast<unsigned>(observed.operation);
    if (f != 4 || suspended[f]) return false;
    const bool left = services::input::physical_button_down(services::input::mouse_button::left);
    const bool right = services::input::physical_button_down(services::input::mouse_button::right);
    const bool shift = services::input::physical_key_down(VK_LSHIFT) || services::input::physical_key_down(VK_RSHIFT);
    const bool applied = jni::get_env()->CallBooleanMethod(bridge, container_act_method, observed.packet,
        static_cast<jint>(action), static_cast<jint>(source), static_cast<jint>(target),
        static_cast<jboolean>(left), static_cast<jboolean>(right), static_cast<jboolean>(shift)) == JNI_TRUE;
    if (platform::jni_context::clear_exception()) {
        suspended[f] = true; cancel(observed.operation);
        OutputDebugStringW(L"[OneStep] Container write failed; capability suspended without replay.\n");
        return false;
    }
    return applied;
}

void services::game::inventory_bridge::tick_mouse(bool active, bool right_tweak, bool left_with_item, bool left_without_item)
{
    if constexpr (mc::target.build_id != 12111) return;
    if (suspended[static_cast<unsigned>(feature::mouse)]) return;
    if (!permitted() || !platform::jni_context::bind_current_thread()) {
        cancel(feature::mouse);
        return;
    }
    if (!connect(jni::get_env())) return;
    auto env = jni::get_env();
    env->CallVoidMethod(bridge, mouse_tick_method, static_cast<jboolean>(active),
        static_cast<jboolean>(right_tweak), static_cast<jboolean>(left_with_item),
        static_cast<jboolean>(left_without_item));
    if (platform::jni_context::clear_exception()) {
        suspended[static_cast<unsigned>(feature::mouse)] = true;
        cancel(feature::mouse);
        OutputDebugStringW(L"[OneStep] Mouse Tweaks Java path failed; feature suspended for this session.\n");
    }
}

void services::game::inventory_bridge::cancel(feature operation)
{
    if (!bridge || !platform::jni_context::bind_current_thread()) return;
    jni::get_env()->CallVoidMethod(bridge, cancel_method, static_cast<jint>(operation));
    platform::jni_context::clear_exception();
}
void services::game::inventory_bridge::shutdown()
{
    if (bridge && platform::jni_context::bind_current_thread()) {
        jni::get_env()->CallVoidMethod(bridge, stop_method); platform::jni_context::clear_exception();
        jni::get_env()->DeleteGlobalRef(bridge);
    }
    bridge = nullptr; prepare_method = cancel_method = stop_method = nullptr;
    container_capture_method = container_act_method = mouse_tick_method = nullptr;
    setup_failed = false; prepared = {}; rejected = {}; suspended = {};
}
