#pragma once

#include <jni.h>
#include <memory>
#include <string>

class jvmti;

namespace platform
{
    // Owned by the native worker. Game/cache objects must die before this session.
    class jvm_session
    {
    public:
        jvm_session();
        ~jvm_session();
        jvm_session(const jvm_session&) = delete;
        jvm_session& operator=(const jvm_session&) = delete;

        bool open();
        JNIEnv* env() const { return env_; }
        jvmti* classes() const { return classes_.get(); }
        const std::string& error() const { return error_; }

    private:
        JavaVM* vm_ = nullptr;
        JNIEnv* env_ = nullptr;
        std::unique_ptr<jvmti> classes_;
        bool attached_ = false;
        bool metadata_ready_ = false;
        std::string error_;
    };
}
