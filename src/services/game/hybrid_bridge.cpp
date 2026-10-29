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
    jmethodID tick_method = nullptr, shutdown_method = nullptr;
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
        if (!failed) OutputDebugStringW(L"[OneStep] Java game bridge unavailable; No Jump Delay suspended.\n");
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
            tick_method = checked(env, env->GetMethodID(type, "tick", "()Z"));
            shutdown_method = checked(env, env->GetMethodID(type, "stop", "()V"));
            auto instance = checked(env, env->NewObject(type, create, parent, static_cast<jint>(mc::target.build_id)));
            bridge = checked(env, env->NewGlobalRef(instance));
            OutputDebugStringW(L"[OneStep] Java game bridge active: No Jump Delay.\n");
            return true;
        }
        catch (...) { fail(); return false; }
    }
}

void services::game::hybrid::tick_no_jump_delay()
{
    if (!permitted() || !platform::jni_context::bind_current_thread()) return;
    jni::frame frame{16};
    auto env = jni::get_env();
    if (!connect(env)) return;
    env->CallBooleanMethod(bridge, tick_method);
    if (env->ExceptionCheck()) fail();
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
    tick_method = shutdown_method = nullptr;
    failed = false;
}
