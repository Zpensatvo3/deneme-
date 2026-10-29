#include "item_bridge.hpp"
#include "../../gui/gui.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../services/input/input_service.hpp"
#include "../../sdk/minecraft/common/client.hpp"
#include "../../../resource.h"
#include <Windows.h>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace
{
    jobject bridge = nullptr;
    jmethodID prepare_method = nullptr;
    jmethodID exp_method = nullptr, use_method = nullptr, stop_method = nullptr;
    bool connection_failed = false;
    std::array<bool, 5> prepared{}, failed{};
    bool permitted()
    {
        const HWND window = gui::get_window();
        return !gui::draw && !gui::loader_visible && !gui::should_unload &&
            window && GetForegroundWindow() == window;
    }
    template<class T> T checked(JNIEnv* env, T value)
    {
        if (env->ExceptionCheck() || !value) throw std::runtime_error("Item JNI bridge");
        return value;
    }
    void fail(unsigned feature)
    {
        platform::jni_context::clear_exception();
        if (!failed[feature]) OutputDebugStringW(L"[OneStep] Item/input capability failed; pending action cancelled, existing backend retained.\n");
        failed[feature] = true;
    }
    bool connect(JNIEnv* env)
    {
        if (connection_failed) return false;
        if (bridge) return true;
        try
        {
            auto game = checked(env, jni::get_cached_jclass<maps::MinecraftClient>());
            auto type = checked(env, env->FindClass("java/lang/Class"));
            auto get_loader = checked(env, env->GetMethodID(type, "getClassLoader", "()Ljava/lang/ClassLoader;"));
            auto parent = checked(env, env->CallObjectMethod(game, get_loader));
            auto loader_type = checked(env, env->FindClass("java/net/URLClassLoader"));
            auto create_loader = checked(env, env->GetMethodID(loader_type, "<init>", "([Ljava/net/URL;Ljava/lang/ClassLoader;)V"));
            auto url_type = checked(env, env->FindClass("java/net/URL"));
            auto urls = checked(env, env->NewObjectArray(0, url_type, nullptr));
            auto loader = checked(env, env->NewObject(loader_type, create_loader, urls, parent));
            HMODULE owner = nullptr;
            if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(&connect), &owner)) throw std::runtime_error("Item module resource");
            auto resource = FindResourceW(owner, MAKEINTRESOURCEW(IDR_ITEM_BRIDGE), RT_RCDATA);
            const auto size = resource ? SizeofResource(owner, resource) : 0;
            auto loaded = resource ? LoadResource(owner, resource) : nullptr;
            auto bytes = loaded ? LockResource(loaded) : nullptr;
            if (!bytes || !size || size > static_cast<DWORD>((std::numeric_limits<jsize>::max)()))
                throw std::runtime_error("Missing ItemBridge resource");
            auto item = checked(env, env->DefineClass("net/onestep/hybrid/ItemBridge", loader,
                static_cast<const jbyte*>(bytes), static_cast<jsize>(size)));
            auto create = checked(env, env->GetMethodID(item, "<init>", "(Ljava/lang/ClassLoader;I)V"));
            prepare_method = checked(env, env->GetMethodID(item, "prepare", "(I)V"));
            exp_method = checked(env, env->GetMethodID(item, "captureExp", "()[Ljava/lang/Object;"));
            use_method = checked(env, env->GetMethodID(item, "useExp", "([Ljava/lang/Object;Z)Z"));
            stop_method = checked(env, env->GetMethodID(item, "stop", "()V"));
            auto instance = checked(env, env->NewObject(item, create, parent, static_cast<jint>(mc::target.build_id)));
            bridge = checked(env, env->NewGlobalRef(instance));
            OutputDebugStringW(L"[OneStep] Java item bridge connected (1.21.11).\n");
            return true;
        }
        catch (...)
        {
            platform::jni_context::clear_exception(); connection_failed = true;
            OutputDebugStringW(L"[OneStep] Item bridge setup failed; existing backends retained.\n");
            return false;
        }
    }
    services::game::hybrid::result prepare(unsigned feature)
    {
        using services::game::hybrid::result;
        if (feature != 2) return result::unavailable;
        if constexpr (mc::target.build_id != 12111) return result::native_fallback;
        if (!permitted() || !platform::jni_context::bind_current_thread()) return result::unavailable;
        if (failed[feature] || !connect(jni::get_env())) return result::native_fallback;
        if (!prepared[feature])
        {
            jni::get_env()->CallVoidMethod(bridge, prepare_method, static_cast<jint>(feature));
            if (platform::jni_context::clear_exception()) { fail(feature); return result::native_fallback; }
            prepared[feature] = true;
        }
        return result::ready;
    }
    template<std::size_t N> void read_packet(JNIEnv* env, jobjectArray packet, jsize count, jsize index, std::array<double, N>& values)
    {
        if (env->GetArrayLength(packet) != count) throw std::runtime_error("Item packet shape");
        auto owner = checked(env, env->GetObjectArrayElement(packet, 0));
        if (!env->IsSameObject(owner, bridge)) throw std::runtime_error("Item packet owner");
        auto value = checked(env, env->GetObjectArrayElement(packet, index));
        auto type = checked(env, env->FindClass("[D"));
        if (!env->IsInstanceOf(value, type) || env->GetArrayLength(static_cast<jdoubleArray>(value)) != N)
            throw std::runtime_error("Item packet data");
        env->GetDoubleArrayRegion(static_cast<jdoubleArray>(value), 0, static_cast<jsize>(N), values.data());
        if (env->ExceptionCheck()) throw std::runtime_error("Item packet read");
        for (double number : values) if (!std::isfinite(number)) throw std::runtime_error("Item nonfinite value");
        if (values[0] != 1) throw std::runtime_error("Item packet schema");
    }
}

services::game::hybrid::result services::game::items::capture_exp(exp_snapshot& out)
{
    out = {};
    const auto backend = prepare(2);
    if (backend != hybrid::result::ready) return backend;
    auto env = jni::get_env();
    try
    {
        auto packet = static_cast<jobjectArray>(env->CallObjectMethod(bridge, exp_method));
        if (env->ExceptionCheck()) throw std::runtime_error("EXP capture");
        if (!packet) return hybrid::result::unavailable;
        std::array<double, 4> values{}; read_packet(env, packet, 6, 5, values);
        if (values[1] <= 0 || (values[2] != 0 && values[2] != 1) || (values[3] != 0 && values[3] != 1))
            throw std::runtime_error("EXP flags");
        out.player = checked(env, env->GetObjectArrayElement(packet, 1));
        out.world = checked(env, env->GetObjectArrayElement(packet, 2));
        out.packet = packet; out.holding_exp = values[2] == 1 || values[3] == 1;
        return hybrid::result::ready;
    }
    catch (...) { fail(2); out = {}; return hybrid::result::unavailable; }
}

bool services::game::items::use_exp(const exp_snapshot& observed)
{
    if (!bridge || failed[2] || !observed.packet || !permitted() ||
        !services::input::physical_button_down(services::input::mouse_button::right)) return false;
    const bool accepted = jni::get_env()->CallBooleanMethod(bridge, use_method, observed.packet, JNI_TRUE) == JNI_TRUE;
    if (platform::jni_context::clear_exception()) { fail(2); return false; }
    return accepted;
}

void services::game::items::shutdown()
{
    if (bridge && platform::jni_context::bind_current_thread())
    {
        jni::get_env()->CallVoidMethod(bridge, stop_method);
        platform::jni_context::clear_exception(); jni::get_env()->DeleteGlobalRef(bridge);
    }
    bridge = nullptr;
    prepare_method = exp_method = use_method = stop_method = nullptr;
    connection_failed = false; prepared = {}; failed = {};
}
