#include "jvm_session.hpp"
#include "../../jvmti/jvmti.hpp"
#include "../../meta_jni.hpp"
#include <Windows.h>

// Compatibility for existing JNI/render adapters.
JavaVM* jvm = nullptr;

platform::jvm_session::jvm_session() = default;

platform::jvm_session::~jvm_session()
{
    if (metadata_ready_) jni::shutdown();
    classes_.reset();
    if (attached_) vm_->DetachCurrentThread();
    jvm = nullptr;
}

bool platform::jvm_session::open()
{
    if (metadata_ready_) return classes_ && static_cast<bool>(*classes_);
    if (vm_) { error_ = "JVM session initialization already attempted"; return false; }
    using get_vms_function = jint(JNICALL*)(JavaVM**, jsize, jsize*);
    const HMODULE library = GetModuleHandleW(L"jvm.dll");
    const auto get_vms = library ? reinterpret_cast<get_vms_function>(
        GetProcAddress(library, "JNI_GetCreatedJavaVMs")) : nullptr;
    if (!get_vms) { error_ = "JNI_GetCreatedJavaVMs export not found"; return false; }

    jsize count = 0;
    if (get_vms(&vm_, 1, &count) != JNI_OK || count < 1 || !vm_)
    { error_ = "Minecraft JVM instance was not available"; return false; }

    const jint status = vm_->GetEnv(reinterpret_cast<void**>(&env_), JNI_VERSION_1_8);
    if (status == JNI_EDETACHED)
    {
        if (vm_->AttachCurrentThread(reinterpret_cast<void**>(&env_), nullptr) != JNI_OK)
        { error_ = "Failed to attach the native JVM thread"; return false; }
        attached_ = true;
    }
    else if (status != JNI_OK || !env_)
    { error_ = "Native JVM thread environment was not available"; return false; }

    jvm = vm_;
    jni::init();
    metadata_ready_ = true;
    jni::set_thread_env(env_);
    classes_ = std::make_unique<jvmti>(vm_);
    if (!*classes_) { error_ = "JVMTI class lookup was not available"; return false; }
    jni::set_custom_find_class([this](const char* name) -> jclass {
        return classes_->find_loaded_class(name);
    });
    return true;
}
