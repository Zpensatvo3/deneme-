#include "hybrid_bridge.hpp"
#include "../../gui/gui.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../sdk/minecraft/common/client.hpp"
#include "../../utils/version_manager.hpp"
#include "../../../resource.h"
#include <Windows.h>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace
{
    // Mutated only on the render thread, or after that thread has joined.
    jobject bridge = nullptr;
    jmethodID capture_method = nullptr, jump_method = nullptr, shutdown_method = nullptr;
    bool failed = false;

    bool permitted()
    {
        const HWND window = gui::get_window();
        return !gui::should_unload && !gui::draw && !gui::loader_visible &&
            window && GetForegroundWindow() == window;
    }

    template<class T> T checked(JNIEnv* env, T value)
    {
        if (env->ExceptionCheck() || !value) throw std::runtime_error("Hybrid JNI binding");
        return value;
    }

    void fail()
    {
        platform::jni_context::clear_exception();
        if (!failed) OutputDebugStringW(L"[OneStep] Java game bridge unavailable; native backend retained.\n");
        failed = true;
    }

    bool connect(JNIEnv* env)
    {
        if (failed) return false;
        if (bridge) return true;
        try
        {
            auto game = checked(env, jni::get_cached_jclass<maps::MinecraftClient>());
            auto class_type = checked(env, env->FindClass("java/lang/Class"));
            auto get_loader = checked(env, env->GetMethodID(class_type, "getClassLoader", "()Ljava/lang/ClassLoader;"));
            auto parent = checked(env, env->CallObjectMethod(game, get_loader));
            auto loader_type = checked(env, env->FindClass("java/net/URLClassLoader"));
            auto constructor = checked(env, env->GetMethodID(loader_type, "<init>", "([Ljava/net/URL;Ljava/lang/ClassLoader;)V"));
            auto url_type = checked(env, env->FindClass("java/net/URL"));
            auto urls = checked(env, env->NewObjectArray(0, url_type, nullptr));
            auto loader = checked(env, env->NewObject(loader_type, constructor, urls, parent));
            HMODULE owner = nullptr;
            if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    reinterpret_cast<LPCWSTR>(&connect), &owner)) throw std::runtime_error("Hybrid module resource");
            auto resource = FindResourceW(owner, MAKEINTRESOURCEW(IDR_GAME_BRIDGE), RT_RCDATA);
            const auto size = resource ? SizeofResource(owner, resource) : 0;
            auto loaded = resource ? LoadResource(owner, resource) : nullptr;
            auto bytes = loaded ? LockResource(loaded) : nullptr;
            if (!bytes || !size || size > static_cast<DWORD>((std::numeric_limits<jsize>::max)()))
                throw std::runtime_error("Missing GameBridge resource");
            auto type = checked(env, env->DefineClass("net/onestep/hybrid/GameBridge", loader,
                static_cast<const jbyte*>(bytes), static_cast<jsize>(size)));
            auto create = checked(env, env->GetMethodID(type, "<init>", "(Ljava/lang/ClassLoader;I)V"));
            capture_method = checked(env, env->GetMethodID(type, "capture", "(I)[Ljava/lang/Object;"));
            jump_method = checked(env, env->GetMethodID(type, "clearJump", "([Ljava/lang/Object;)Z"));
            shutdown_method = checked(env, env->GetMethodID(type, "stop", "()V"));
            auto instance = checked(env, env->NewObject(type, create, parent, static_cast<jint>(mc::target.build_id)));
            bridge = checked(env, env->NewGlobalRef(instance));
            OutputDebugStringW(L"[OneStep] Java game bridge active: No Jump Delay (1.21.11).\n");
            return true;
        }
        catch (...) { fail(); return false; }
    }
}

services::game::hybrid::result services::game::hybrid::capture(feature operation, snapshot& out)
{
    out = {};
        if constexpr (mc::target.build_id != 12111) return result::native_fallback;
    if (!permitted() || !platform::jni_context::bind_current_thread()) return result::unavailable;
    auto env = jni::get_env();
    if (!connect(env)) return result::native_fallback;
    try
    {
        auto packet = static_cast<jobjectArray>(env->CallObjectMethod(bridge, capture_method, static_cast<jint>(operation)));
        if (env->ExceptionCheck()) throw std::runtime_error("Hybrid capture");
        if (!packet) return result::unavailable; // Menu, wrong thread or no world is not a binding failure.
        if (env->GetArrayLength(packet) != 4) throw std::runtime_error("Hybrid packet schema");
        auto owner = checked(env, env->GetObjectArrayElement(packet, 0));
        if (!env->IsSameObject(owner, bridge)) throw std::runtime_error("Hybrid packet owner");
        auto player = checked(env, env->GetObjectArrayElement(packet, 1));
        auto world = checked(env, env->GetObjectArrayElement(packet, 2));
        auto values = checked(env, env->GetObjectArrayElement(packet, 3));
        auto array_type = checked(env, env->FindClass("[D"));
        if (!env->IsInstanceOf(values, array_type)) throw std::runtime_error("Hybrid data type");
        auto array = static_cast<jdoubleArray>(values);
        if (env->GetArrayLength(array) != out.data.size()) throw std::runtime_error("Hybrid data size");
        env->GetDoubleArrayRegion(array, 0, static_cast<jsize>(out.data.size()), out.data.data());
        if (env->ExceptionCheck()) throw std::runtime_error("Hybrid data read");
        for (const double value : out.data) if (!std::isfinite(value)) throw std::runtime_error("Hybrid nonfinite data");
        if (out.data[0] != 1 || out.data[1] != static_cast<jint>(operation) ||
            out.data[2] < 0 || out.data[2] > 1023 || std::floor(out.data[2]) != out.data[2] ||
            out.data[10] <= 0) throw std::runtime_error("Hybrid invalid schema");
        out.packet = packet; out.player = player; out.world = world;
        return result::ready;
    }
    catch (...) { fail(); out = {}; return result::unavailable; }
}

bool services::game::hybrid::apply(feature operation, const snapshot& observed)
{
    if (!bridge || failed || !observed.packet || !permitted()) return false;
    auto env = jni::get_env();
    if (!env) return false;
    if (operation != feature::no_jump_delay) return false;
    const auto method = jump_method;
    const bool applied = env->CallBooleanMethod(bridge, method, observed.packet) == JNI_TRUE;
    if (env->ExceptionCheck()) { fail(); return false; }
    return applied;
}

void services::game::hybrid::shutdown()
{
    if (bridge && platform::jni_context::bind_current_thread())
    {
        auto env = jni::get_env();
        env->CallVoidMethod(bridge, shutdown_method);
        platform::jni_context::clear_exception();
        env->DeleteGlobalRef(bridge);
    }
    bridge = nullptr;
    capture_method = jump_method = shutdown_method = nullptr;
    failed = false;
}
