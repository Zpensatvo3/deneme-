#include "utility_bridge.hpp"
#include "../../gui/gui.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../sdk/minecraft/common/client.hpp"
#include "../../../resource.h"
#include <Windows.h>
#include <array>
#include <limits>
#include <stdexcept>

namespace
{
    jobject bridge = nullptr;
    jmethodID prepare_method = nullptr, update_method = nullptr, release_method = nullptr, stop_method = nullptr;
    bool connection_failed = false;
    std::array<bool, 4> prepared{}, rejected{}, quarantined{};

    template<class T> T checked(JNIEnv* env, T value)
    {
        if (env->ExceptionCheck() || !value) throw std::runtime_error("Utility JNI binding");
        return value;
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
                reinterpret_cast<LPCWSTR>(&connect), &owner)) throw std::runtime_error("Utility module resource");
            auto resource = FindResourceW(owner, MAKEINTRESOURCEW(IDR_UTILITY_BRIDGE), RT_RCDATA);
            const auto size = resource ? SizeofResource(owner, resource) : 0;
            auto loaded = resource ? LoadResource(owner, resource) : nullptr;
            auto bytes = loaded ? LockResource(loaded) : nullptr;
            if (!bytes || !size || size > static_cast<DWORD>((std::numeric_limits<jsize>::max)()))
                throw std::runtime_error("Missing UtilityBridge resource");
            auto utility = checked(env, env->DefineClass("net/onestep/hybrid/UtilityBridge", loader,
                static_cast<const jbyte*>(bytes), static_cast<jsize>(size)));
            auto create = checked(env, env->GetMethodID(utility, "<init>", "(Ljava/lang/ClassLoader;I)V"));
            prepare_method = checked(env, env->GetMethodID(utility, "prepare", "(I)V"));
            update_method = checked(env, env->GetMethodID(utility, "update", "(IZIZ)V"));
            release_method = checked(env, env->GetMethodID(utility, "release", "(I)Z"));
            stop_method = checked(env, env->GetMethodID(utility, "stop", "()V"));
            auto instance = checked(env, env->NewObject(utility, create, parent, static_cast<jint>(mc::target.build_id)));
            bridge = checked(env, env->NewGlobalRef(instance));
            OutputDebugStringW(L"[OneStep] Java utility bridge connected (1.21.11).\n");
            return true;
        }
        catch (...)
        {
            platform::jni_context::clear_exception();
            connection_failed = true;
            OutputDebugStringW(L"[OneStep] Utility Java setup failed; legacy backend retained.\n");
            return false;
        }
    }
}

services::game::hybrid::result services::game::utility::update(feature operation, bool active, int value, bool physical_use_held)
{
    using hybrid::result;
    if constexpr (mc::target.build_id != 12111) return result::native_fallback;
    const auto index = static_cast<std::size_t>(operation);
    if (index < 1 || index > 3) return result::unavailable;
    if (rejected[index] || connection_failed) return result::native_fallback;
    if (!platform::jni_context::bind_current_thread()) return result::unavailable;
    const HWND window = gui::get_window();
    active = active && !gui::should_unload && !gui::loader_visible;
    if (operation != feature::full_bright)
        active = active && !gui::draw && window && GetForegroundWindow() == window;
    // Do not install or resolve bindings just because a disabled module renders for cleanup.
    if (!active && !bridge) return result::unavailable;
    jni::frame frame{48};
    auto env = jni::get_env();
    if (!connect(env)) return result::native_fallback;
    if (quarantined[index])
    {
        // A mutator may already have run before throwing. Clean up first; never
        // allow simultaneous Java-owned sprint/gamma and legacy worker writes.
        if (release(operation)) { rejected[index] = true; quarantined[index] = false; }
        return result::unavailable;
    }
    if (!active)
    {
        release(operation);
        return result::ready;
    }
    if (!prepared[index])
    {
        env->CallVoidMethod(bridge, prepare_method, static_cast<jint>(operation));
        if (platform::jni_context::clear_exception())
        {
            rejected[index] = true;
            OutputDebugStringW(L"[OneStep] Utility capability unavailable; only that module uses the legacy backend.\n");
            return result::native_fallback;
        }
        prepared[index] = true;
    }
    env->CallVoidMethod(bridge, update_method, static_cast<jint>(operation), static_cast<jboolean>(active),
        static_cast<jint>(value), static_cast<jboolean>(physical_use_held));
    if (platform::jni_context::clear_exception())
    {
        quarantined[index] = true;
        OutputDebugStringW(L"[OneStep] Utility runtime failure; module suspended until owned state is released.\n");
        return result::unavailable;
    }
    return result::ready;
}

bool services::game::utility::release(feature operation)
{
    if (!bridge) return true;
    if (!platform::jni_context::bind_current_thread()) return false;
    jni::frame frame{8};
    const bool released = jni::get_env()->CallBooleanMethod(bridge, release_method, static_cast<jint>(operation)) == JNI_TRUE;
    return !platform::jni_context::clear_exception() && released;
}

void services::game::utility::shutdown()
{
    if (bridge && platform::jni_context::bind_current_thread())
    {
        auto env = jni::get_env();
        env->CallVoidMethod(bridge, stop_method);
        platform::jni_context::clear_exception();
        env->DeleteGlobalRef(bridge);
    }
    bridge = nullptr;
    prepare_method = update_method = release_method = stop_method = nullptr;
    prepared = {}; rejected = {}; quarantined = {};
    connection_failed = false;
}
