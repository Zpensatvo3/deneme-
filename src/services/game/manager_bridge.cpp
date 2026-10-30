#include "manager_bridge.hpp"
#include "../../modules/core/manager.hpp"
#include "../../gui/gui.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../sdk/minecraft/common/client.hpp"
#include "../../utils/version_manager.hpp"
#include "../../../resource.h"
#include <Windows.h>
#include <atomic>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace
{
    // Initialized before callbacks start; released only after every callback joins.
    jclass core = nullptr;
    jmethodID snapshot_method = nullptr, request_method = nullptr, stop_method = nullptr;
    jlong token = 0;
    struct exception_guard {
        ~exception_guard() {
            if (platform::jni_context::clear_exception()) {
                gui::runtime_ready.store(false, std::memory_order_release);
                gui::should_unload.store(true, std::memory_order_release);
            }
        }
    };
    template<class T> T checked(JNIEnv* env, T value)
    {
        if (env->ExceptionCheck() || !value) throw std::runtime_error("Module manager JNI binding failed");
        return value;
    }
    std::string bytes(JNIEnv* env, jbyteArray array)
    {
        if (!array) return {};
        const auto count = env->GetArrayLength(array);
        if (count < 0 || count > 6 * 96 * 1024 + 4096) throw std::runtime_error("Module manager payload limit");
        std::string result(static_cast<std::size_t>(count), '\0');
        env->GetByteArrayRegion(array, 0, count, reinterpret_cast<jbyte*>(result.data()));
        if (env->ExceptionCheck()) throw std::runtime_error("Module manager UTF-8 response failed");
        return result;
    }
    std::string snapshot(std::uint64_t revision)
    {
        if (!core || !platform::jni_context::bind_current_thread()) throw std::runtime_error("Module manager unavailable");
        jni::frame frame{16}; auto env = jni::get_env();
        exception_guard cleanup;
        auto response = static_cast<jbyteArray>(env->CallStaticObjectMethod(core, snapshot_method, token, static_cast<jlong>(revision)));
        if (env->ExceptionCheck()) throw std::runtime_error("Module snapshot failed");
        return bytes(env, response);
    }
    std::string request(const std::string& command)
    {
        if (!core || !platform::jni_context::bind_current_thread()) throw std::runtime_error("Module manager unavailable");
        jni::frame frame{16}; auto env = jni::get_env();
        exception_guard cleanup;
        auto input = checked(env, env->NewByteArray(static_cast<jsize>(command.size())));
        env->SetByteArrayRegion(input, 0, static_cast<jsize>(command.size()), reinterpret_cast<const jbyte*>(command.data()));
        checked(env, input);
        auto response = static_cast<jbyteArray>(env->CallStaticObjectMethod(core, request_method, token, input));
        checked(env, response); return bytes(env, response);
    }
}

bool services::game::module_manager::initialize()
{
    if (core) return false;
    try {
        if (!platform::jni_context::bind_current_thread()) return false;
        jni::frame frame{128}; auto env = jni::get_env();
        auto game = checked(env, jni::get_cached_jclass<maps::MinecraftClient>());
        auto class_type = checked(env, env->FindClass("java/lang/Class"));
        auto get_loader = checked(env, env->GetMethodID(class_type, "getClassLoader", "()Ljava/lang/ClassLoader;"));
        auto parent = checked(env, env->CallObjectMethod(game, get_loader));
        auto loader_type = checked(env, env->FindClass("java/lang/ClassLoader"));
        auto load = checked(env, env->GetMethodID(loader_type, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;"));
        auto name = checked(env, env->NewStringUTF("net.onestep.hybrid.ManagerBridge"));
        auto type = static_cast<jclass>(env->CallObjectMethod(parent, load, name));
        if (env->ExceptionCheck()) { env->ExceptionClear(); type = nullptr; }
        if (!type) {
            HMODULE owner = nullptr;
            if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(&initialize), &owner)) throw std::runtime_error("Module resource owner unavailable");
            auto resource = FindResourceW(owner, MAKEINTRESOURCEW(IDR_MANAGER_CORE), RT_RCDATA);
            auto size = resource ? SizeofResource(owner, resource) : 0;
            auto loaded = resource ? LoadResource(owner, resource) : nullptr;
            auto data = loaded ? static_cast<const unsigned char*>(LockResource(loaded)) : nullptr;
            if (!data || size < 8 || std::memcmp(data, "WMC1", 4) != 0) throw std::runtime_error("Missing module manager bundle");
            std::size_t position = 4;
            auto number = [&]() -> std::uint32_t {
                if (position + 4 > size) throw std::runtime_error("Truncated module manager bundle");
                std::uint32_t result = 0;
                for (int i = 0; i < 4; ++i) result = result * 256 + data[position++];
                return result;
            };
            auto count = number(); if (count > 128) throw std::runtime_error("Module manager class limit");
            for (std::uint32_t i = 0; i < count; ++i) {
                auto length = number();
                if (!length || length > 256 || position + length > size) throw std::runtime_error("Invalid module manager class name");
                std::string binary_name(reinterpret_cast<const char*>(data + position), length); position += length;
                auto class_size = number();
                if (!class_size || class_size > 1024 * 1024 || position + class_size > size) throw std::runtime_error("Invalid module manager class size");
                // All manager classes are defined in the GAME parent, never a per-feature child loader.
                auto defined = checked(env, env->DefineClass(binary_name.c_str(), parent,
                    reinterpret_cast<const jbyte*>(data + position), static_cast<jsize>(class_size)));
                env->DeleteLocalRef(defined); position += class_size;
            }
            if (position != size) throw std::runtime_error("Trailing module manager bundle bytes");
            type = checked(env, static_cast<jclass>(env->CallObjectMethod(parent, load, name)));
        }
        core = static_cast<jclass>(checked(env, env->NewGlobalRef(type)));
        auto start = checked(env, env->GetStaticMethodID(core, "start", "(JII)Z"));
        snapshot_method = checked(env, env->GetStaticMethodID(core, "snapshot", "(JJ)[B"));
        request_method = checked(env, env->GetStaticMethodID(core, "request", "(J[B)[B"));
        stop_method = checked(env, env->GetStaticMethodID(core, "stop", "(J)V"));
        token = static_cast<jlong>(GetTickCount64() ^ reinterpret_cast<std::uintptr_t>(&initialize));
        if (!token) token = 1;
        if (env->CallStaticBooleanMethod(core, start, token, modules::catalog::manager_abi, static_cast<jint>(mc::target.build_id)) != JNI_TRUE || env->ExceptionCheck())
            throw std::runtime_error("Module manager ABI, target or session handshake failed");
        if (!modules::manager::connect({&snapshot, &request})) throw std::runtime_error("Module manager snapshot could not be synchronized");
        return true;
    } catch (...) {
        platform::jni_context::clear_exception(); shutdown();
        OutputDebugStringA("[OneStep] Module manager initialization failed; runtime disabled.\n");
        return false;
    }
}
void services::game::module_manager::shutdown()
{
    modules::manager::disconnect();
    if (core && platform::jni_context::bind_current_thread()) {
        auto env = jni::get_env();
        if (stop_method && token) env->CallStaticVoidMethod(core, stop_method, token);
        platform::jni_context::clear_exception(); env->DeleteGlobalRef(core);
    }
    core = nullptr; snapshot_method = request_method = stop_method = nullptr; token = 0;
}
