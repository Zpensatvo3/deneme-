#include "jni_context.hpp"

#include "../../meta_jni.hpp"

extern JavaVM* jvm;

bool platform::jni_context::bind_current_thread()
{
    if (jni::get_env())
        return true;

    JNIEnv* env = nullptr;
    if (!jvm ||
        jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_8) != JNI_OK ||
        !env)
        return false;

    jni::set_thread_env(env);
    return jni::get_env() != nullptr;
}


bool platform::jni_context::clear_exception()
{
    JNIEnv* env = jni::get_env();
    if (!env || env->ExceptionCheck() != JNI_TRUE)
        return false;

    env->ExceptionClear();
    return true;
}
