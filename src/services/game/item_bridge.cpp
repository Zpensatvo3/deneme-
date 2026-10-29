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
    jmethodID update_method = nullptr, stop_method = nullptr;
    bool connection_failed = false;
    bool failed = false;
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
    void fail()
    {
        platform::jni_context::clear_exception();
        if (!failed) OutputDebugStringW(L"[OneStep] Fast EXP Java controller failed; suspended.\n");
        failed = true;
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
            update_method = checked(env, env->GetMethodID(item, "update", "(ZZZII)V"));
            stop_method = checked(env, env->GetMethodID(item, "stop", "()V"));
            auto instance = checked(env, env->NewObject(item, create, parent, static_cast<jint>(mc::target.build_id)));
            bridge = checked(env, env->NewGlobalRef(instance));
            OutputDebugStringW(L"[OneStep] Java item bridge connected.\n");
            return true;
        }
        catch (...)
        {
            platform::jni_context::clear_exception(); connection_failed = true;
            OutputDebugStringW(L"[OneStep] Item bridge setup failed; Fast EXP suspended.\n");
            return false;
        }
    }
}

void services::game::items::update(bool enabled, int min_cps, int max_cps)
{
    if (!platform::jni_context::bind_current_thread()) return;
    const bool allowed = permitted();
    const bool held = services::input::physical_button_down(services::input::mouse_button::right);
    if (!bridge && (!enabled || !allowed || !held)) return;
    jni::frame frame{32};
    auto env = jni::get_env();
    if (!connect(env) || failed) return;
    env->CallVoidMethod(bridge, update_method, static_cast<jboolean>(enabled),
        static_cast<jboolean>(allowed), static_cast<jboolean>(held),
        static_cast<jint>(min_cps), static_cast<jint>(max_cps));
    if (env->ExceptionCheck()) fail();
}

void services::game::items::shutdown()
{
    if (bridge && platform::jni_context::bind_current_thread())
    {
        jni::get_env()->CallVoidMethod(bridge, stop_method);
        platform::jni_context::clear_exception(); jni::get_env()->DeleteGlobalRef(bridge);
    }
    bridge = nullptr;
    update_method = stop_method = nullptr;
    connection_failed = false; failed = false;
}
