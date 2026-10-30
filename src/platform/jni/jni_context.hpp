#pragma once

#include <jni.h>

namespace platform::jni_context
{
    // Binds the current native thread to meta_jni only when the JVM already
    // knows that thread. It deliberately does not attach arbitrary workers.
    bool bind_current_thread();

    // Returns true when a pending Java exception was found and cleared.
    bool clear_exception();
}
