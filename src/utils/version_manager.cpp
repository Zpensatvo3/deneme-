#include "version_manager.hpp"
#include "../jvmti/jvmti.hpp"
#include "../meta_jni.hpp"

namespace mc
{
    namespace
    {
        version current = version::unknown;
        std::string detected_name = "unknown";
        std::string runtime_label = "NATIVE RUNTIME  //  unknown";

        // Fabric API names are independent of Minecraft's intermediary mappings.
        // Resolve the game's loader through JVMTI on the attached native thread.
        std::string read_fabric_version(JNIEnv* env, jvmti* classes)
        {
            if (!env || !classes || !*classes) return {};
            jni::frame local_frame{32};
            const auto checked = [env]<class Result>(Result result) -> Result {
                if (env->ExceptionCheck()) { env->ExceptionClear(); return {}; }
                return result;
            };
            auto loader = classes->find_loaded_class("net/fabricmc/loader/api/FabricLoader");
            if (!loader) return {};
            auto get_instance = checked(env->GetStaticMethodID(loader, "getInstance", "()Lnet/fabricmc/loader/api/FabricLoader;"));
            auto get_mod = checked(env->GetMethodID(loader, "getModContainer", "(Ljava/lang/String;)Ljava/util/Optional;"));
            if (!get_instance || !get_mod) return {};
            auto instance = checked(env->CallStaticObjectMethod(loader, get_instance));
            auto id = checked(env->NewStringUTF("minecraft"));
            if (!instance || !id) return {};
            auto optional = checked(env->CallObjectMethod(instance, get_mod, id));
            auto optional_type = checked(env->FindClass("java/util/Optional"));
            if (!optional || !optional_type) return {};
            auto or_null = checked(env->GetMethodID(optional_type, "orElse", "(Ljava/lang/Object;)Ljava/lang/Object;"));
            if (!or_null) return {};
            auto container = checked(env->CallObjectMethod(optional, or_null, nullptr));
            if (!container) return {};
            auto container_type = checked(env->GetObjectClass(container));
            auto metadata_method = container_type ? checked(env->GetMethodID(container_type, "getMetadata", "()Lnet/fabricmc/loader/api/metadata/ModMetadata;")) : nullptr;
            if (!metadata_method) return {};
            auto metadata = checked(env->CallObjectMethod(container, metadata_method));
            auto metadata_type = metadata ? checked(env->GetObjectClass(metadata)) : nullptr;
            auto version_method = metadata_type ? checked(env->GetMethodID(metadata_type, "getVersion", "()Lnet/fabricmc/loader/api/Version;")) : nullptr;
            if (!version_method) return {};
            auto value = checked(env->CallObjectMethod(metadata, version_method));
            auto value_type = value ? checked(env->GetObjectClass(value)) : nullptr;
            auto friendly = value_type ? checked(env->GetMethodID(value_type, "getFriendlyString", "()Ljava/lang/String;")) : nullptr;
            if (!friendly) return {};
            auto name = static_cast<jstring>(checked(env->CallObjectMethod(value, friendly)));
            if (!name) return {};
            const char* utf8 = checked(env->GetStringUTFChars(name, nullptr));
            if (!utf8) return {};
            std::string result(utf8);
            env->ReleaseStringUTFChars(name, utf8);
            return result;
        }
    }

    version get_version() { return current; }
    const char* get_version_name() { return detected_name.c_str(); }
    const char* get_runtime_label() { return runtime_label.c_str(); }
    void set_version(version value)
    {
        current = value;
        detected_name = version_name(value);
        runtime_label = "NATIVE RUNTIME  //  " + detected_name;
    }
    version detect(JNIEnv* env, jvmti* classes)
    {
        const auto name = read_fabric_version(env, classes);
        set_version(parse_version(name));
        if (!name.empty()) detected_name = name;
        runtime_label = "NATIVE RUNTIME  //  " + detected_name;
        return current;
    }
}
