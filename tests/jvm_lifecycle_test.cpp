// Starts an isolated Java 25 VM. Never attaches to Minecraft or loads the DLL.
#include "../src/jvmti/jvmti.hpp"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <thread>
#include <vector>

namespace
{
    constexpr const char* fixture_name = "net/onestep/test/ReTransformFixture";
    jclass fixture = nullptr;
    int changes = 0;
    void JNICALL transform(jvmtiEnv* tools, JNIEnv* env, jclass redefining, jobject,
        const char* name, jobject, jint length, const unsigned char* original, jint* size, unsigned char** output)
    {
        if (!redefining || !name || std::strcmp(name, fixture_name) || !env->IsSameObject(redefining, fixture)) return;
        int offset = -1;
        for (int i = 0; i + 2 < length; ++i)
        {
            // javac's exact body for this fixture: BIPUSH 41, IRETURN.
            if (original[i] != 0x10 || original[i + 1] != 41 || original[i + 2] != 0xac) continue;
            assert(offset == -1); offset = i + 1;
        }
        assert(offset >= 0);
        assert(tools->Allocate(length, output) == JVMTI_ERROR_NONE);
        std::memcpy(*output, original, length); (*output)[offset] = 42; *size = length; ++changes;
    }
}

int main(int argc, char** argv)
{
    assert(argc == 2);
    std::ifstream stream(argv[1], std::ios::binary);
    std::vector<char> bytes((std::istreambuf_iterator<char>(stream)), {});
    assert(bytes.size() > 8 && static_cast<unsigned char>(bytes[7]) == 69);
    JavaVM* vm = nullptr; JNIEnv* env = nullptr;
    JavaVMOption option{const_cast<char*>("-Xcheck:jni"), nullptr};
    JavaVMInitArgs options{}; options.version = JNI_VERSION_1_8; options.nOptions = 1; options.options = &option;
    assert(JNI_CreateJavaVM(&vm, reinterpret_cast<void**>(&env), &options) == JNI_OK);
    JavaVM* discovered = nullptr; jsize count = 0;
    assert(JNI_GetCreatedJavaVMs(&discovered, 1, &count) == JNI_OK && count == 1 && discovered == vm);
    jni::init(); jni::set_thread_env(env);
    {
        jni::frame frame{64};
        fixture = env->DefineClass(fixture_name, nullptr, reinterpret_cast<const jbyte*>(bytes.data()), static_cast<jsize>(bytes.size()));
        assert(!env->ExceptionCheck() && fixture);
        auto value = env->GetStaticMethodID(fixture, "value", "()I");
        assert(!env->ExceptionCheck() && value);
        const auto read = [&] { auto result = env->CallStaticIntMethod(fixture, value); assert(!env->ExceptionCheck()); return result; };
        assert(read() == 41);
        {
            jvmti lookup(vm);
            assert(lookup);
            auto loaded = lookup.find_loaded_class(fixture_name);
            assert(loaded && env->IsSameObject(loaded.object_instance, fixture));
            assert(lookup.get_class_signature(loaded) == std::string("L") + fixture_name + ";");
            for (int round = 0; round < 2; ++round)
            {
                jvmtiEnv* tools = nullptr;
                assert(vm->GetEnv(reinterpret_cast<void**>(&tools), JVMTI_VERSION_1_2) == JNI_OK && tools);
                jvmtiCapabilities capabilities{}; capabilities.can_retransform_classes = 1;
                assert(tools->AddCapabilities(&capabilities) == JVMTI_ERROR_NONE);
                jboolean modifiable = JNI_FALSE;
                assert(tools->IsModifiableClass(fixture, &modifiable) == JVMTI_ERROR_NONE && modifiable);
                jvmtiEventCallbacks callbacks{}; callbacks.ClassFileLoadHook = &transform;
                assert(tools->SetEventCallbacks(&callbacks, sizeof(callbacks)) == JVMTI_ERROR_NONE);
                assert(tools->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, nullptr) == JVMTI_ERROR_NONE);
                assert(tools->RetransformClasses(1, &fixture) == JVMTI_ERROR_NONE);
                assert(read() == 42 && changes == round + 1);
                assert(tools->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, nullptr) == JVMTI_ERROR_NONE);
                assert(tools->RetransformClasses(1, &fixture) == JVMTI_ERROR_NONE && read() == 41);
                assert(tools->DisposeEnvironment() == JVMTI_ERROR_NONE);
            }
        }
        std::thread attached([&] {
            JNIEnv* thread_env = nullptr;
            assert(vm->GetEnv(reinterpret_cast<void**>(&thread_env), JNI_VERSION_1_8) == JNI_EDETACHED);
            assert(vm->AttachCurrentThread(reinterpret_cast<void**>(&thread_env), nullptr) == JNI_OK);
            jni::set_thread_env(thread_env); assert(jni::get_env() == thread_env);
            { jvmti lookup(vm); assert(lookup && lookup.find_loaded_class(fixture_name)); }
            assert(vm->DetachCurrentThread() == JNI_OK);
        });
        attached.join(); fixture = nullptr;
    }
    jni::shutdown();
    assert(vm->DestroyJavaVM() == JNI_OK);
    std::cout << "PASS Java 25: JVM discovery, thread attach/detach, production class lookup, retransform/restore/reinstall and shutdown\n";
}
