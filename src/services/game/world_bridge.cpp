#include "world_bridge.hpp"
#include "../../gui/gui.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../input/input_service.hpp"
#include "../../sdk/minecraft/common/client.hpp"
#include "../../../resource.h"
#include <Windows.h>
#include <cmath>
#include <stdexcept>
#include <cstring>
#include <cstdio>
#include <jvmti.h>

extern JavaVM* jvm;
#include "../../modules/combat/trigger_engine.hpp"
#include "../../modules/combat/jump_reset_config.hpp"
#include "../../modules/combat/settings/auto_totem_settings.hpp"
#include "../../modules/combat/shield_breaker.hpp"
#include "../../friends/friends.hpp"
#include "anti_bot_service.hpp"
#include "attack_coordinator.hpp"
#include "client_context.hpp"

namespace
{
    jobject bridge = nullptr;
    jmethodID prepare_id = nullptr;
    jmethodID pearl_id = nullptr, begin_id = nullptr, use_id = nullptr, release_pearl_id = nullptr, stop_id = nullptr;
    jmethodID trigger_tick_id = nullptr, trigger_cancel_id = nullptr;
    jmethodID jump_tick_id = nullptr, jump_cancel_id = nullptr;
    const std::atomic<std::uint64_t>* jump_reset_source = nullptr;
    std::uint64_t jump_expected_generation = 0;
    jmethodID totem_tick_id = nullptr, totem_cancel_id = nullptr;
    const std::atomic<std::uint64_t>* totem_reset_source = nullptr;
    std::uint64_t totem_expected_generation = 0;
    std::array<bool, 8> prepared{}, rejected{}, suspended{};
    const std::atomic<std::uint64_t>* trigger_reset_source = nullptr;
    std::uint64_t trigger_expected_generation = 0;
    int trigger_helper = 0;
    bool failed = false;
    void report_java_failure(JNIEnv* env, const char* operation)
    {
        std::fprintf(stderr,"[OneStep] WorldBridge %s failed; Java combat controllers paused.\n",operation);
        if (env && env->ExceptionCheck()) env->ExceptionDescribe();
    }
    jvmtiEnv* tick_tooling = nullptr;
    jclass tick_client_class = nullptr, tick_transformer_class = nullptr;
    jmethodID tick_transform_id = nullptr, tick_install_events_id = nullptr;
    std::atomic_bool tick_transforming{false};
    bool tick_hooks_installed = false, tick_hooks_failed = false, tick_hook_changed = false;
    void JNICALL tick_class_bytes(jvmtiEnv* tooling, JNIEnv* env, jclass redefining, jobject,
        const char* name, jobject, jint size, const unsigned char* bytes, jint* new_size, unsigned char** new_bytes)
    {
        if (!tick_transforming.load(std::memory_order_acquire) || !redefining || !name ||
            std::strcmp(name,ONESTEP_MC_NAME("net/minecraft/class_310")) != 0 || !env->IsSameObject(redefining,tick_client_class)) return;
        if (env->PushLocalFrame(8) != JNI_OK) { if (env->ExceptionCheck()) env->ExceptionClear(); return; }
        auto original = env->NewByteArray(size);
        if (original) env->SetByteArrayRegion(original,0,size,reinterpret_cast<const jbyte*>(bytes));
        auto result = original && !env->ExceptionCheck() ? static_cast<jbyteArray>(env->CallStaticObjectMethod(
            tick_transformer_class,tick_transform_id,original,static_cast<jint>(mc::target.build_id))) : nullptr;
        if (result && !env->ExceptionCheck()) {
            const auto count = env->GetArrayLength(result); unsigned char* copy = nullptr;
            if (count > 0 && count < size*4+65536 && tooling->Allocate(count,&copy) == JVMTI_ERROR_NONE) {
                env->GetByteArrayRegion(result,0,count,reinterpret_cast<jbyte*>(copy));
                if (!env->ExceptionCheck()) { *new_size=count; *new_bytes=copy; tick_hook_changed=true; }
                else tooling->Deallocate(copy);
            }
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->PopLocalFrame(nullptr);
    }
    void shutdown_tick_hooks(JNIEnv* env)
    {
        tick_transforming.store(false,std::memory_order_release);
        if (tick_tooling) {
            tick_tooling->SetEventNotificationMode(JVMTI_DISABLE,JVMTI_EVENT_CLASS_FILE_LOAD_HOOK,nullptr);
            if (tick_hook_changed && tick_client_class) tick_tooling->RetransformClasses(1,&tick_client_class);
            tick_tooling->DisposeEnvironment(); tick_tooling=nullptr;
        }
        platform::jni_context::clear_exception();
        for (auto* ref : {&tick_client_class,&tick_transformer_class}) if (*ref) { env->DeleteGlobalRef(*ref); *ref=nullptr; }
        tick_transform_id=nullptr; tick_hooks_installed=tick_hook_changed=false;
    }
    template<class T> T tick_checked(JNIEnv* env,T value) {
        if (!value || env->ExceptionCheck()) throw std::runtime_error("Client tick event binding");
        return value;
    }
    jclass define_tick_class(JNIEnv* env,jobject loader,const char* binary,int resource_id)
    {
        HMODULE owner=nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&define_tick_class),&owner)) throw std::runtime_error("Client tick resource owner");
        auto resource=FindResourceW(owner,MAKEINTRESOURCEW(resource_id),RT_RCDATA);
        auto size=resource ? SizeofResource(owner,resource) : 0;
        auto loaded=resource ? LoadResource(owner,resource) : nullptr;
        auto bytes=loaded ? LockResource(loaded) : nullptr;
        if (!bytes || !size || size > 0x7fffffffU) throw std::runtime_error("Client tick event resource");
        return tick_checked(env,env->DefineClass(binary,loader,static_cast<const jbyte*>(bytes),static_cast<jsize>(size)));
    }
    bool install_tick_hooks(JNIEnv* env)
    {
        if (tick_hooks_installed) return true;
        if (tick_hooks_failed || !jvm) return false;
        try {
            auto client=tick_checked(env,jni::get_cached_jclass<maps::MinecraftClient>());
            auto klass=tick_checked(env,env->FindClass("java/lang/Class"));
            auto get_loader=tick_checked(env,env->GetMethodID(klass,"getClassLoader","()Ljava/lang/ClassLoader;"));
            auto parent=tick_checked(env,env->CallObjectMethod(client,get_loader));
            auto for_name=tick_checked(env,env->GetStaticMethodID(klass,"forName","(Ljava/lang/String;ZLjava/lang/ClassLoader;)Ljava/lang/Class;"));
            auto events_name=tick_checked(env,env->NewStringUTF("net.onestep.hybrid.ClientTickRelay"));
            auto events=static_cast<jclass>(env->CallStaticObjectMethod(klass,for_name,events_name,JNI_TRUE,parent));
            if (platform::jni_context::clear_exception()) events=nullptr;
            if (!events) define_tick_class(env,parent,"net/onestep/hybrid/ClientTickRelay",IDR_CLIENT_TICK_EVENTS);
            auto bridge_type=tick_checked(env,env->GetObjectClass(bridge));
            auto loader=tick_checked(env,env->CallObjectMethod(bridge_type,get_loader));
            auto transformer=define_tick_class(env,loader,"net/onestep/hybrid/ClientTickTransformer",IDR_CLIENT_TICK_TRANSFORMER);
            tick_transformer_class=static_cast<jclass>(tick_checked(env,env->NewGlobalRef(transformer)));
            tick_client_class=static_cast<jclass>(tick_checked(env,env->NewGlobalRef(client)));
            tick_transform_id=tick_checked(env,env->GetStaticMethodID(transformer,"transform","([BI)[B"));
            if (jvm->GetEnv(reinterpret_cast<void**>(&tick_tooling),JVMTI_VERSION_1_2) != JNI_OK || !tick_tooling)
                throw std::runtime_error("Client tick JVMTI environment");
            jvmtiCapabilities caps{}; caps.can_retransform_classes=1;
            if (tick_tooling->AddCapabilities(&caps) != JVMTI_ERROR_NONE) throw std::runtime_error("Client tick retransform capability");
            jboolean modifiable=JNI_FALSE;
            if (tick_tooling->IsModifiableClass(client,&modifiable) != JVMTI_ERROR_NONE || !modifiable) throw std::runtime_error("Client tick client modifiability");
            jvmtiEventCallbacks callbacks{}; callbacks.ClassFileLoadHook=&tick_class_bytes;
            if (tick_tooling->SetEventCallbacks(&callbacks,sizeof(callbacks)) != JVMTI_ERROR_NONE) throw std::runtime_error("Client tick callback");
            tick_transforming.store(true,std::memory_order_release);
            auto result=tick_tooling->SetEventNotificationMode(JVMTI_ENABLE,JVMTI_EVENT_CLASS_FILE_LOAD_HOOK,nullptr);
            if (result == JVMTI_ERROR_NONE) result=tick_tooling->RetransformClasses(1,&tick_client_class);
            tick_tooling->SetEventNotificationMode(JVMTI_DISABLE,JVMTI_EVENT_CLASS_FILE_LOAD_HOOK,nullptr);
            tick_transforming.store(false,std::memory_order_release);
            if (result != JVMTI_ERROR_NONE || !tick_hook_changed) throw std::runtime_error("Client tick client hooks");
            env->CallVoidMethod(bridge,tick_install_events_id);
            if (env->ExceptionCheck()) throw std::runtime_error("Client tick Java event handlers");
            tick_hooks_installed=true; return true;
        } catch (...) {
            platform::jni_context::clear_exception(); shutdown_tick_hooks(env); tick_hooks_failed=true;
            OutputDebugStringW(L"[OneStep] Client tick hook unavailable; Auto Totem inactive.\n");
            return false;
        }
    }
    bool permitted() {
        const HWND window = gui::get_window();
        return window && GetForegroundWindow() == window &&
            !gui::draw && gui::runtime_ready.load(std::memory_order_acquire) && !gui::should_unload;
    }
    jboolean JNICALL jump_permitted(JNIEnv*, jobject, jlong generation) {
        return permitted() && jump_reset_source && static_cast<std::uint64_t>(generation) == jump_expected_generation &&
            jump_reset_source->load(std::memory_order_acquire) == jump_expected_generation ? JNI_TRUE : JNI_FALSE;
    }
    jboolean JNICALL totem_permitted(JNIEnv*, jobject, jlong generation) {
        return permitted() && totem_reset_source && static_cast<std::uint64_t>(generation) == totem_expected_generation &&
            totem_reset_source->load(std::memory_order_acquire) == totem_expected_generation ? JNI_TRUE : JNI_FALSE;
    }
    void finish_trigger_helper(jobject p, jobject w, bool consumed) {
        if (trigger_helper == 0) return;
        services::game::client_context context;
        services::game::capture_client_context(context, false, false);
        const bool same = p && w && context.player && context.world &&
            jni::get_env()->IsSameObject(p, context.player.object_instance) &&
            jni::get_env()->IsSameObject(w, context.world.object_instance);
        const maps::ClientPlayerEntity player = same ? context.player : maps::ClientPlayerEntity{};
        const maps::ClientPlayerInteractionManager manager = same ? context.interaction : maps::ClientPlayerInteractionManager{};
        if (trigger_helper == 2) modules::shield_breaker::finish_trigger_preparation(player, manager, consumed && same);
        trigger_helper = 0;
    }
    jlong JNICALL trigger_service(JNIEnv* env, jobject, jint operation, jobject p, jobject w, jobject e, jint value, jlong epoch) {
        try {
            if (operation == 4 || operation == 5) {
                finish_trigger_helper(p, w, operation == 4);
                if (operation == 5) services::combat::release_trigger();
                return 1;
            }
            const bool valid = permitted() && trigger_reset_source &&
                trigger_reset_source->load(std::memory_order_acquire) == trigger_expected_generation;
            if (!valid) return operation == 8 || operation == 2 || operation == 6 ? -1 : 0;
            switch (operation) {
            case 0: {
                // Shared OneStep filters remain the single source of truth.
                maps::Entity target(e, false);
                return target && !friends::is_friend(target) && !services::anti_bot::is_bot(target) &&
                    !platform::jni_context::clear_exception();
            }
            case 1: services::combat::claim_trigger(value, static_cast<std::uint64_t>(epoch)); return 1;
            case 2:
                trigger_helper = 2;
                if (modules::shield_breaker::prepare_for_trigger_attack()) return modules::shield_breaker::trigger_prepared_slot();
                return -1;
            case 6: {
                maps::ClientPlayerEntity player(p, false); maps::ClientWorld world(w, false);
                int slot = modules::shield_breaker::starting_slot_if_owned(player, world, value);
                return slot;
            }
            case 7: return value == (trigger_helper == 2 ? modules::shield_breaker::trigger_prepared_slot() : -1);
            case 8: return static_cast<std::uint64_t>(epoch) == trigger_expected_generation ?
                static_cast<jlong>(services::input::manual_attack_generation()) : -1;
            case 10: return -1; // Shield already handled by the selected axe.
            default: return 0;
            }
        } catch (...) {
            platform::jni_context::clear_exception();
            if (!env->ExceptionCheck()) {
                auto exception = env->FindClass("java/lang/IllegalStateException");
                if (exception) env->ThrowNew(exception, "Trigger shared service failed");
            }
            return -1;
        }
    }
    template<class T> T checked(JNIEnv* env, T value) {
        if (!value || env->ExceptionCheck()) throw std::runtime_error("World JNI binding");
        return value;
    }
    // Exact names for the selected binary, using the existing SDK profile.
    // Java-only thread/item/query methods and press count use official Mojang names on 26.1.x.
    jobjectArray runtime_names(JNIEnv* env)
    {
        struct binding { const char* source; const char* runtime; };
        static constexpr binding names[]{
#if ONESTEP_MINECRAFT_26_1
            {"comp_2909", "swingSource"},
#else
            {"comp_2909", "comp_2909"},
#endif
#if ONESTEP_MINECRAFT_26_1
            {"method_2383", "slotClicked"},
            {"method_25402", "mouseClicked"},
#else
            {"method_2383", "method_2383"},
            {"method_25402", "method_25402"},
#endif
            {"net/minecraft/class_465", ONESTEP_MC_NAME("net/minecraft/class_465")},
            {"field_2797", ONESTEP_MC_NAME("field_2797")},
            {"net/minecraft/class_490", ONESTEP_MC_NAME("net/minecraft/class_490")},
            {"net/minecraft/class_1703", ONESTEP_MC_NAME("net/minecraft/class_1703")},
            {"net/minecraft/class_1713", ONESTEP_MC_NAME("net/minecraft/class_1713")},
            {"field_8288", ONESTEP_MC_NAME("field_8288")},
            {"field_7791", ONESTEP_MC_NAME("field_7791")},
            {"field_7512", ONESTEP_MC_NAME("field_7512")},
            {"field_7763", ONESTEP_MC_NAME("field_7763")},
            {"method_2906", ONESTEP_MC_NAME("method_2906")},
#if ONESTEP_MINECRAFT_26_1
            {"method_1507", "setScreen"},
            {"method_25419", "onClose"},
#else
            {"method_1507", "method_1507"},
            {"method_25419", "method_25419"},
#endif

            {"method_6047", ONESTEP_MC_NAME("method_6047")},
            {"method_6079", ONESTEP_MC_NAME("method_6079")},

            {"net/minecraft/class_1656", ONESTEP_MC_NAME("net/minecraft/class_1656")},
            {"net/minecraft/class_1282", ONESTEP_MC_NAME("net/minecraft/class_1282")},
            {"field_1903", ONESTEP_MC_NAME("field_1903")},
            {"field_6235", ONESTEP_MC_NAME("field_6235")},
            {"field_6012", ONESTEP_MC_NAME("field_6012")},
            {"field_6228", ONESTEP_MC_NAME("field_6228")},
            {"field_7478", ONESTEP_MC_NAME("field_7478")},
            {"field_7479", ONESTEP_MC_NAME("field_7479")},
            {"method_6113", ONESTEP_MC_NAME("method_6113")},
            {"method_31549", ONESTEP_MC_NAME("method_31549")},
            {"method_6081", ONESTEP_MC_NAME("method_6081")},
            {"method_5529", ONESTEP_MC_NAME("method_5529")},
            {"net/minecraft/class_312", ONESTEP_MC_NAME("net/minecraft/class_312")},
            {"field_1729", ONESTEP_MC_NAME("field_1729")},
            {"field_1791", ONESTEP_MC_NAME("field_1791")},
            {"method_1601", ONESTEP_MC_NAME("method_1601")},
            {"net/minecraft/class_11910", ONESTEP_MC_NAME("net/minecraft/class_11910")},
#if ONESTEP_MINECRAFT_26_1
            {"field_1657", "ALL"},
            {"field_1653", "isDown"},
#else
            {"field_1657", "field_1657"},
            {"field_1653", "field_1653"},
#endif
            {"net/minecraft/class_1041", ONESTEP_MC_NAME("net/minecraft/class_1041")},
            {"method_22683", ONESTEP_MC_NAME("method_22683")},
            {"method_4490", ONESTEP_MC_NAME("method_4490")},
            {"net/minecraft/class_1309", ONESTEP_MC_NAME("net/minecraft/class_1309")},
            {"net/minecraft/class_1308", ONESTEP_MC_NAME("net/minecraft/class_1308")},
            {"net/minecraft/class_1429", ONESTEP_MC_NAME("net/minecraft/class_1429")},
            {"net/minecraft/class_1743", ONESTEP_MC_NAME("net/minecraft/class_1743")},
            {"net/minecraft/class_6880", ONESTEP_MC_NAME("net/minecraft/class_6880")},
            {"net/minecraft/class_1294", ONESTEP_MC_NAME("net/minecraft/class_1294")},
            {"field_6017", ONESTEP_MC_NAME("field_6017")},
            {"field_6273", ONESTEP_MC_NAME("field_6273")},
            {"field_6031", ONESTEP_MC_NAME("field_6031")},
            {"method_5767", ONESTEP_MC_NAME("method_5767")},
            {"method_7261", ONESTEP_MC_NAME("method_7261")},
            {"method_24828", ONESTEP_MC_NAME("method_24828")},
            {"method_5799", ONESTEP_MC_NAME("method_5799")},
            {"method_5681", ONESTEP_MC_NAME("method_5681")},
            {"method_6128", ONESTEP_MC_NAME("method_6128")},
            {"method_6101", ONESTEP_MC_NAME("method_6101")},
            {"method_5765", ONESTEP_MC_NAME("method_5765")},
            {"method_5624", ONESTEP_MC_NAME("method_5624")},
            {"method_6059", ONESTEP_MC_NAME("method_6059")},
            {"field_5919", ONESTEP_MC_NAME("field_5919")},
            {"method_6039", ONESTEP_MC_NAME("method_6039")},
            {"method_1560", ONESTEP_MC_NAME("method_1560")},
            {"method_76763", ONESTEP_MC_NAME("method_76763")},
            {"net/minecraft/class_6880$class_6883", ONESTEP_MC_NAME("net/minecraft/class_6880$class_6883")},
            {"net/minecraft/class_6862", ONESTEP_MC_NAME("net/minecraft/class_6862")},
            {"net/minecraft/class_3489", ONESTEP_MC_NAME("net/minecraft/class_3489")},
            {"method_40131", ONESTEP_MC_NAME("method_40131")},
            {"method_40220", ONESTEP_MC_NAME("method_40220")},
            {"field_42611", ONESTEP_MC_NAME("field_42611")},

            {"net/minecraft/class_1268", ONESTEP_MC_NAME("net/minecraft/class_1268")},
            {"net/minecraft/class_1297", ONESTEP_MC_NAME("net/minecraft/class_1297")},
            {"net/minecraft/class_1657", ONESTEP_MC_NAME("net/minecraft/class_1657")},
            {"net/minecraft/class_1661", ONESTEP_MC_NAME("net/minecraft/class_1661")},
            {"net/minecraft/class_1792", ONESTEP_MC_NAME("net/minecraft/class_1792")},
            {"net/minecraft/class_1799", ONESTEP_MC_NAME("net/minecraft/class_1799")},
            {"net/minecraft/class_1802", ONESTEP_MC_NAME("net/minecraft/class_1802")},
            {"net/minecraft/class_2371", ONESTEP_MC_NAME("net/minecraft/class_2371")},
            {"net/minecraft/class_239", ONESTEP_MC_NAME("net/minecraft/class_239")},
            {"net/minecraft/class_243", ONESTEP_MC_NAME("net/minecraft/class_243")},
            {"net/minecraft/class_304", ONESTEP_MC_NAME("net/minecraft/class_304")},
            {"net/minecraft/class_310", ONESTEP_MC_NAME("net/minecraft/class_310")},
            {"net/minecraft/class_315", ONESTEP_MC_NAME("net/minecraft/class_315")},
            {"net/minecraft/class_3675$class_306", ONESTEP_MC_NAME("net/minecraft/class_3675$class_306")},
            {"net/minecraft/class_3675$class_307", ONESTEP_MC_NAME("net/minecraft/class_3675$class_307")},
            {"net/minecraft/class_3966", ONESTEP_MC_NAME("net/minecraft/class_3966")},
            {"net/minecraft/class_437", ONESTEP_MC_NAME("net/minecraft/class_437")},
            {"net/minecraft/class_636", ONESTEP_MC_NAME("net/minecraft/class_636")},
            {"net/minecraft/class_638", ONESTEP_MC_NAME("net/minecraft/class_638")},
            {"net/minecraft/class_746", ONESTEP_MC_NAME("net/minecraft/class_746")},
            {"field_1350", ONESTEP_MC_NAME("field_1350")},
            {"field_1351", ONESTEP_MC_NAME("field_1351")},
            {"field_1352", ONESTEP_MC_NAME("field_1352")},
            {"method_18798", ONESTEP_MC_NAME("method_18798")},
            {"field_1655", ONESTEP_MC_NAME("field_1655")},
#if ONESTEP_MINECRAFT_26_1
            {"field_1661", "clickCount"},
#else
            {"field_1661", "field_1661"},
#endif
            {"field_1668", ONESTEP_MC_NAME("field_1668")},
            {"field_1671", ONESTEP_MC_NAME("field_1671")},
            {"field_1672", ONESTEP_MC_NAME("field_1672")},
            {"field_1687", ONESTEP_MC_NAME("field_1687")},
            {"field_1690", ONESTEP_MC_NAME("field_1690")},
            {"field_1700", ONESTEP_MC_NAME("field_1700")},
            {"field_1724", ONESTEP_MC_NAME("field_1724")},
            {"field_1755", ONESTEP_MC_NAME("field_1755")},
            {"field_17592", ONESTEP_MC_NAME("field_17592")},
            {"field_1761", ONESTEP_MC_NAME("field_1761")},
            {"field_1765", ONESTEP_MC_NAME("field_1765")},
            {"field_22467", ONESTEP_MC_NAME("field_22467")},
            {"field_5808", ONESTEP_MC_NAME("field_5808")},
            {"field_7545", ONESTEP_MC_NAME("field_7545")},
            {"method_1434", ONESTEP_MC_NAME("method_1434")},
            {"method_1442", ONESTEP_MC_NAME("method_1442")},
            {"method_1444", ONESTEP_MC_NAME("method_1444")},
#if ONESTEP_MINECRAFT_26_1
            {"method_18854", "isSameThread"},
#else
            {"method_18854", "method_18854"},
#endif
            {"method_23481", ONESTEP_MC_NAME("method_23481")},
            {"method_2911", ONESTEP_MC_NAME("method_2911")},
            {"method_2918", ONESTEP_MC_NAME("method_2918")},
            {"method_31548", ONESTEP_MC_NAME("method_31548")},
            {"method_5438", ONESTEP_MC_NAME("method_5438")},
            {"method_55754", ONESTEP_MC_NAME("method_55754")},
            {"method_55755", ONESTEP_MC_NAME("method_55755")},
            {"method_5628", ONESTEP_MC_NAME("method_5628")},
            {"method_5805", ONESTEP_MC_NAME("method_5805")},
            {"method_6104", ONESTEP_MC_NAME("method_6104")},
            {"method_6115", ONESTEP_MC_NAME("method_6115")},
#if ONESTEP_MINECRAFT_26_1
            {"method_7909", "getItem"},
#else
            {"method_7909", "method_7909"},
#endif
            {"method_7960", ONESTEP_MC_NAME("method_7960")},
        };
        auto string_type = checked(env, env->FindClass("java/lang/String"));
        auto values = checked(env, env->NewObjectArray(static_cast<jsize>(std::size(names)*2), string_type, nullptr));
        jsize index = 0;
        for (const auto& name : names)
        {
            for (const auto text : {name.source, name.runtime})
            {
                auto value = checked(env, env->NewStringUTF(text));
                env->SetObjectArrayElement(values, index++, value);
                env->DeleteLocalRef(value);
                if (env->ExceptionCheck()) throw std::runtime_error("World mapping array");
            }
        }
        return values;
    }
    bool connect(JNIEnv* env) {
        if (failed) return false;
        if (bridge) return true;
        try {
            auto game = checked(env, jni::get_cached_jclass<maps::MinecraftClient>());
            auto klass = checked(env, env->FindClass("java/lang/Class"));
            auto get_loader = checked(env, env->GetMethodID(klass, "getClassLoader", "()Ljava/lang/ClassLoader;"));
            auto parent = checked(env, env->CallObjectMethod(game, get_loader));
            auto loader_type = checked(env, env->FindClass("java/net/URLClassLoader"));
            auto create_loader = checked(env, env->GetMethodID(loader_type, "<init>", "([Ljava/net/URL;Ljava/lang/ClassLoader;)V"));
            auto url = checked(env, env->FindClass("java/net/URL"));
            auto urls = checked(env, env->NewObjectArray(0, url, nullptr));
            auto loader = checked(env, env->NewObject(loader_type, create_loader, urls, parent));
            HMODULE owner = nullptr;
            if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(&connect), &owner)) throw std::runtime_error("World resource owner");
            auto resource = FindResourceW(owner, MAKEINTRESOURCEW(IDR_WORLD_BRIDGE), RT_RCDATA);
            const auto size = resource ? SizeofResource(owner, resource) : 0;
            auto loaded = resource ? LoadResource(owner, resource) : nullptr;
            auto bytes = loaded ? LockResource(loaded) : nullptr;
            if (!bytes || !size || size > 0x7fffffffU) throw std::runtime_error("World resource");
            auto type = checked(env, env->DefineClass("net/onestep/hybrid/WorldBridge", loader, static_cast<const jbyte*>(bytes), static_cast<jsize>(size)));
            JNINativeMethod callbacks[]{
                {const_cast<char*>("triggerService"), const_cast<char*>("(ILjava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;IJ)J"), reinterpret_cast<void*>(&trigger_service)},
                {const_cast<char*>("jumpPermitted"), const_cast<char*>("(J)Z"), reinterpret_cast<void*>(&jump_permitted)},
                {const_cast<char*>("totemPermitted"), const_cast<char*>("(J)Z"), reinterpret_cast<void*>(&totem_permitted)}
            };
            if (env->RegisterNatives(type, callbacks, static_cast<jint>(std::size(callbacks))) != JNI_OK || env->ExceptionCheck()) throw std::runtime_error("World native service binding");
            totem_tick_id = checked(env,env->GetMethodID(type,"tickTotem","(J)V"));
            totem_cancel_id = checked(env,env->GetMethodID(type,"cancelTotem","()V"));
            tick_install_events_id = checked(env,env->GetMethodID(type,"installTickEvents","()V"));
            jump_tick_id = checked(env, env->GetMethodID(type, "tickJump", "(J)V"));
            jump_cancel_id = checked(env, env->GetMethodID(type, "cancelJump", "()V"));
            trigger_tick_id = checked(env, env->GetMethodID(type, "tickTrigger", "(JJ)V"));
            trigger_cancel_id = checked(env, env->GetMethodID(type, "cancelTrigger", "()V"));
            auto create = checked(env, env->GetMethodID(type, "<init>", "(Ljava/lang/ClassLoader;I[Ljava/lang/String;)V"));
            prepare_id = checked(env, env->GetMethodID(type, "prepare", "(I)V"));
            pearl_id = checked(env, env->GetMethodID(type, "capturePearl", "()[Ljava/lang/Object;"));
            begin_id = checked(env, env->GetMethodID(type, "beginPearl", "([Ljava/lang/Object;I)Z"));
            use_id = checked(env, env->GetMethodID(type, "usePearl", "()Z"));
            release_pearl_id = checked(env, env->GetMethodID(type, "releasePearl", "()Z"));
            stop_id = checked(env, env->GetMethodID(type, "stop", "()V"));
            auto object = checked(env, env->NewObject(type, create, parent, static_cast<jint>(mc::target.build_id), runtime_names(env)));
            bridge = checked(env, env->NewGlobalRef(object)); return true;
        } catch (...) {
            report_java_failure(env,"initialization");
            platform::jni_context::clear_exception(); failed = true;
            OutputDebugStringW(L"[OneStep] World bridge setup unavailable; Java-only combat controllers inactive.\n"); return false;
        }
    }
    services::game::hybrid::result prepare(unsigned feature) {
        using services::game::hybrid::result;
        if (feature != 2 && feature != 4 && feature != 5 && feature != 7) return result::unavailable;
        if constexpr (mc::target.build_id != 12111) if (feature < 3) return result::native_fallback;
        if (!permitted() || !platform::jni_context::bind_current_thread() || suspended[feature]) return result::unavailable;
        if (rejected[feature] || !connect(jni::get_env())) return result::native_fallback;
        if (!prepared[feature]) {
            jni::get_env()->CallVoidMethod(bridge, prepare_id, static_cast<jint>(feature));
            if (jni::get_env()->ExceptionCheck()) {
                report_java_failure(jni::get_env(),"feature preparation");
                platform::jni_context::clear_exception(); rejected[feature] = true; return result::native_fallback;
            }
            prepared[feature] = true;
        }
        return result::ready;
    }
    bool checked_result(unsigned feature, bool value) {
        auto env=jni::get_env();
        if (!env) return false;
        if (!env->ExceptionCheck()) return value;
        report_java_failure(env,"runtime operation");
        platform::jni_context::clear_exception();
        suspended[feature] = true;
        OutputDebugStringW(L"[OneStep] World capability suspended after runtime failure; no action replay.\n");
        return false;
    }
    jobjectArray packet(jmethodID id, int size, int data_index, std::array<double, 13>& values) {
        auto env = jni::get_env();
        auto p = static_cast<jobjectArray>(env->CallObjectMethod(bridge, id));
        if (env->ExceptionCheck()) throw std::runtime_error("World capture");
        if (!p) return nullptr;
        if (env->GetArrayLength(p) != size || !env->IsSameObject(checked(env, env->GetObjectArrayElement(p, 0)), bridge)) throw std::runtime_error("World packet");
        auto raw = checked(env, env->GetObjectArrayElement(p, data_index));
        auto doubles = checked(env, env->FindClass("[D"));
        if (!env->IsInstanceOf(raw, doubles) || env->GetArrayLength(static_cast<jarray>(raw)) != 13) throw std::runtime_error("World values");
        env->GetDoubleArrayRegion(static_cast<jdoubleArray>(raw), 0, 13, values.data());
        for (double v : values) if (!std::isfinite(v)) throw std::runtime_error("World number");
        if (env->ExceptionCheck() || values[0] != 1 || values[1] <= 0 || std::floor(values[1]) != values[1]) throw std::runtime_error("World schema");
        for (unsigned i = 3; i < 13; ++i) if (values[i] != 0 && values[i] != 1) throw std::runtime_error("World flag");
        return p;
    }

}

services::game::hybrid::result services::game::world_bridge::capture_pearl(pearl_snapshot& out)
{
    out = {}; const auto backend = prepare(2); if (backend != hybrid::result::ready) return backend;
    try {
        std::array<double, 13> values{}; out.packet = packet(pearl_id, 7, 6, values);
        if (!out.packet) return hybrid::result::unavailable;
        if (values[2] < 0 || values[2] > 8 || std::floor(values[2]) != values[2]) throw std::runtime_error("Pearl slot");
        out.selected = static_cast<int>(values[2]); out.owned = values[3] == 1;
        for (unsigned i = 0; i < 9; ++i) out.pearls[i] = values[4 + i] == 1;
        return hybrid::result::ready;
    } catch (...) { checked_result(2, false); suspended[2] = true; out = {}; return hybrid::result::unavailable; }
}
bool services::game::world_bridge::begin_pearl(const pearl_snapshot& observed, int slot)
{
    if (!bridge || !observed.packet || !permitted() || suspended[2]) return false;
    return checked_result(2, jni::get_env()->CallBooleanMethod(bridge, begin_id, observed.packet, static_cast<jint>(slot)) == JNI_TRUE);
}
bool services::game::world_bridge::use_pearl()
{
    return bridge && permitted() && !suspended[2] && checked_result(2, jni::get_env()->CallBooleanMethod(bridge, use_id) == JNI_TRUE);
}
bool services::game::world_bridge::release_pearl()
{
    if (!bridge) return true;
    if (!platform::jni_context::bind_current_thread()) return false;
    const bool released = jni::get_env()->CallBooleanMethod(bridge, release_pearl_id) == JNI_TRUE;
    return !platform::jni_context::clear_exception() && released;
}
void services::game::world_bridge::tick_trigger(bool active,
    const std::atomic<std::uint64_t>& generation)
{
    if (!active || !permitted()) { cancel_trigger(); return; }
    if (!platform::jni_context::bind_current_thread()) { services::combat::release_trigger(); return; }
    jni::frame frame{192};
    if (prepare(4) != hybrid::result::ready) { cancel_trigger(); return; }
    trigger_expected_generation = generation.load(std::memory_order_acquire);
    trigger_reset_source = &generation;
    auto env = jni::get_env();
    env->CallVoidMethod(bridge, trigger_tick_id,
        static_cast<jlong>(trigger_expected_generation), static_cast<jlong>(services::input::manual_attack_generation()));
    if (!checked_result(4, true)) cancel_trigger();
}
void services::game::world_bridge::cancel_trigger()
{
    trigger_reset_source = nullptr;
    trigger_expected_generation = 0;
    services::combat::release_trigger();
    if (!bridge || !trigger_cancel_id || !platform::jni_context::bind_current_thread()) return;
    jni::frame frame{96};
    jni::get_env()->CallVoidMethod(bridge, trigger_cancel_id);
    if (platform::jni_context::clear_exception()) {
        finish_trigger_helper(nullptr, nullptr, false);
        suspended[4] = true;
    }
}
void services::game::world_bridge::tick_jump(bool active,
    const std::atomic<std::uint64_t>& generation)
{
    if (!active || !permitted()) { cancel_jump(); return; }
    if (!platform::jni_context::bind_current_thread()) return;
    jni::frame frame{128};
    if (prepare(5) != hybrid::result::ready) { cancel_jump(); return; }
    jump_expected_generation = generation.load(std::memory_order_acquire);
    jump_reset_source = &generation;
    jni::get_env()->CallVoidMethod(bridge, jump_tick_id, static_cast<jlong>(jump_expected_generation));
    if (!checked_result(5, true)) cancel_jump();
}
void services::game::world_bridge::cancel_jump()
{
    jump_reset_source = nullptr; jump_expected_generation = 0;
    if (!bridge || !jump_cancel_id || !platform::jni_context::bind_current_thread()) return;
    jni::frame frame{96};
    jni::get_env()->CallVoidMethod(bridge, jump_cancel_id);
    if (platform::jni_context::clear_exception()) suspended[5] = true;
}
void services::game::world_bridge::tick_totem(bool active,
    const std::atomic<std::uint64_t>& generation, std::uint64_t expected_generation)
{
    if (!active || !permitted() || generation.load(std::memory_order_acquire) != expected_generation) { cancel_totem(); return; }
    if (!platform::jni_context::bind_current_thread()) return;
    jni::frame frame{64};
    if (!connect(jni::get_env()) || !install_tick_hooks(jni::get_env()) || prepare(7) != hybrid::result::ready) { cancel_totem(); return; }
    totem_reset_source = &generation; totem_expected_generation = expected_generation;
    auto env = jni::get_env();
    env->CallVoidMethod(bridge,totem_tick_id,static_cast<jlong>(expected_generation));
    if (!checked_result(7,true)) cancel_totem();
}
void services::game::world_bridge::cancel_totem()
{
    totem_reset_source=nullptr; totem_expected_generation=0;
    if (!bridge || !totem_cancel_id || !platform::jni_context::bind_current_thread()) return;
    jni::frame frame{32};
    jni::get_env()->CallVoidMethod(bridge,totem_cancel_id);
    if (platform::jni_context::clear_exception()) suspended[7]=true;
}

void services::game::world_bridge::shutdown()
{
    totem_reset_source = nullptr; totem_expected_generation = 0;
    jump_reset_source = nullptr; jump_expected_generation = 0;
    if (bridge && platform::jni_context::bind_current_thread()) {
        jni::get_env()->CallVoidMethod(bridge, stop_id); platform::jni_context::clear_exception();
        shutdown_tick_hooks(jni::get_env());
        auto type = jni::get_env()->GetObjectClass(bridge);
        if (type) { jni::get_env()->UnregisterNatives(type); jni::get_env()->DeleteLocalRef(type); }
        platform::jni_context::clear_exception(); jni::get_env()->DeleteGlobalRef(bridge);
    }
    bridge = nullptr; prepare_id = nullptr;
    pearl_id = begin_id = use_id = release_pearl_id = stop_id = nullptr;
    trigger_tick_id = trigger_cancel_id = nullptr; trigger_reset_source = nullptr; trigger_helper = 0;
    jump_tick_id = jump_cancel_id = nullptr;
    totem_tick_id = totem_cancel_id = nullptr;
    tick_install_events_id=nullptr;
    tick_hooks_failed=false;
    services::combat::release_trigger();
    prepared = {}; rejected = {}; suspended = {}; failed = false;
}
