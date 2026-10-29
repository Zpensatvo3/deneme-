#pragma once
#include <jni.h>
#include <string>
#include "../platform/adapter/target_version.hpp"

class jvmti;

namespace mc
{
    version get_version();
    const char* get_version_name();
    const char* get_runtime_label();
    bool is_1_21_11();

    // Reads Fabric metadata; unknown versions never select a supported target.
    version detect(JNIEnv* env, jvmti* jvmti_env);

    // Explicit override if needed
    void set_version(version ver);
}
