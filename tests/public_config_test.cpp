#include "../src/modules/core/manager.hpp"
#include "../src/modules/core/public_config.hpp"
#include "../src/modules/core/settings_store.hpp"
#include "../src/modules/misc/settings/fast_exp_settings.hpp"
#include <jni.h>
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

namespace {
    JNIEnv* env = nullptr;
    jclass core = nullptr;
    jmethodID snapshot_id = nullptr, request_id = nullptr;
    constexpr jlong token = 19;
    void checked() {
        if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); throw std::runtime_error("JNI manager test"); }
    }
    std::string response(jbyteArray array) {
        checked(); if (!array) return {};
        std::string result(static_cast<std::size_t>(env->GetArrayLength(array)), '\0');
        env->GetByteArrayRegion(array, 0, static_cast<jsize>(result.size()), reinterpret_cast<jbyte*>(result.data()));
        checked(); env->DeleteLocalRef(array); return result;
    }
    std::string snapshot(std::uint64_t revision) {
        return response(static_cast<jbyteArray>(env->CallStaticObjectMethod(core, snapshot_id, token, static_cast<jlong>(revision))));
    }
    std::string request(const std::string& command) {
        auto input = env->NewByteArray(static_cast<jsize>(command.size())); checked();
        env->SetByteArrayRegion(input, 0, static_cast<jsize>(command.size()), reinterpret_cast<const jbyte*>(command.data()));
        auto output = static_cast<jbyteArray>(env->CallStaticObjectMethod(core, request_id, token, input));
        checked(); env->DeleteLocalRef(input); return response(output);
    }
    std::uint32_t number(const std::vector<char>& input, std::size_t& at) {
        assert(at + 4 <= input.size()); std::uint32_t value = 0;
        for (int i = 0; i < 4; ++i) value = value * 256 + static_cast<unsigned char>(input[at++]);
        return value;
    }
}

int main(int argc, char** argv) {
    assert(argc == 3);
#ifdef _WIN32
    auto library = LoadLibraryA(argv[1]); assert(library);
    auto create = reinterpret_cast<jint(JNICALL*)(JavaVM**, void**, void*)>(GetProcAddress(library, "JNI_CreateJavaVM"));
#else
    auto library = dlopen(argv[1], RTLD_NOW | RTLD_GLOBAL); assert(library);
    auto create = reinterpret_cast<jint(JNICALL*)(JavaVM**, void**, void*)>(dlsym(library, "JNI_CreateJavaVM"));
#endif
    assert(create); JavaVM* vm = nullptr;
    JavaVMOption options[1]{}; options[0].optionString = const_cast<char*>("-Xcheck:jni");
    JavaVMInitArgs args{}; args.version = JNI_VERSION_1_8; args.nOptions = 1; args.options = options;
    assert(create(&vm, reinterpret_cast<void**>(&env), &args) == JNI_OK);
    auto classloader = env->FindClass("java/net/URLClassLoader"); checked();
    auto constructor = env->GetMethodID(classloader, "<init>", "([Ljava/net/URL;Ljava/lang/ClassLoader;)V"); checked();
    auto url = env->FindClass("java/net/URL"); checked();
    auto urls = env->NewObjectArray(0, url, nullptr); checked();
    auto parent = env->NewObject(classloader, constructor, urls, nullptr); checked();
    std::ifstream stream(argv[2], std::ios::binary);
    std::vector<char> bundle{std::istreambuf_iterator<char>(stream), {}};
    assert(bundle.size() > 8 && std::string(bundle.data(), 4) == "WMC1");
    std::size_t at = 4; const auto count = number(bundle, at);
    for (std::uint32_t index = 0; index < count; ++index) {
        auto length = number(bundle, at); assert(at + length <= bundle.size());
        const std::string name(bundle.data() + at, length); at += length;
        auto size = number(bundle, at); assert(at + size <= bundle.size());
        auto type = env->DefineClass(name.c_str(), parent, reinterpret_cast<const jbyte*>(bundle.data() + at), size); checked();
        assert(type); if (name == "net/onestep/hybrid/ManagerBridge") core = type; else env->DeleteLocalRef(type);
        at += size;
    }
    assert(at == bundle.size() && core);
    auto start = env->GetStaticMethodID(core, "start", "(JII)Z"); checked();
    auto stop = env->GetStaticMethodID(core, "stop", "(J)V"); checked();
    snapshot_id = env->GetStaticMethodID(core, "snapshot", "(JJ)[B"); checked();
    request_id = env->GetStaticMethodID(core, "request", "(J[B)[B"); checked();
    auto load = env->GetMethodID(classloader, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;"); checked();
    auto name = env->NewStringUTF("net.onestep.hybrid.ManagerBridge"); checked();
    for (int i = 0; i < 2; ++i) {
        auto child = env->NewObject(classloader, constructor, urls, parent); checked();
        auto shared = env->CallObjectMethod(child, load, name); checked();
        assert(env->IsSameObject(shared, core)); env->DeleteLocalRef(shared); env->DeleteLocalRef(child);
    }
    for (int target : {12111, 26100, 26101, 26102}) {
        assert(env->CallStaticBooleanMethod(core, start, token, 1, target) == JNI_TRUE); checked();
        assert(modules::manager::connect({snapshot, request}));
        assert(modules::manager::read()->modules.size() == 36);
        const auto captured = modules::manager::read();
        assert(modules::manager::enable("sprint", true));
        assert(!captured->modules.at("sprint").enabled && modules::manager::state("sprint").enabled);
        const auto revision = modules::manager::read()->revision;
        assert(modules::manager::enable("sprint", true).revision == revision);
        assert(modules::manager::refresh() && modules::manager::read()->revision == revision);
        assert(!modules::manager::keybind("sprint", 256));
        assert(modules::manager::keybind("sprint", 269));
        {
            modules::manager::view_scope callback;
            assert(modules::manager::state("sprint").enabled);
            assert(modules::manager::enable("sprint", false));
            assert(modules::manager::state("sprint").enabled); // callback is pinned
        }
        assert(!modules::manager::state("sprint").enabled);
        assert(modules::manager::enable("sprint", true));
        std::string message;
        const auto document = modules::export_public_config();
        assert(!document.empty() && modules::apply_public_config(document, message));
        assert(document == modules::export_public_config());
        const auto before = modules::manager::read();
        assert(!modules::apply_public_config(R"({"modules":[{"id":"sprint","enabled":false,"keybind":65,"settings":{}},{"id":"jump_reset","enabled":true,"keybind":0,"settings":{"mode":99}}]})", message));
        assert(modules::manager::read()->revision == before->revision && modules::manager::state("sprint").enabled);
        modules::settings::store<module_settings::fast_exp> settings{"fast_exp"};
        assert(settings.read().min_cps == 16);
        auto copy = settings.read(); copy.min_cps = 1; assert(settings.read().min_cps == 16);
        assert(!settings.update([](auto& candidate) { candidate.min_cps = 3; return false; }));
        assert(!settings.update([](auto&) { return true; }));
        assert(settings.update([](auto& candidate) { candidate.min_cps = 45; return true; }));
        assert(settings.read().min_cps == 45);
        assert(!modules::manager::settings("fast_exp", R"({"min_cps":2})", false, before->revision));
        auto settings_before = settings.read();
        assert(!settings.update([](auto& candidate) { candidate.min_cps = 1000001; return true; }));
        assert(settings.read() == settings_before);
        assert(modules::manager::disable_all());
        for (const auto& [id, state] : modules::manager::read()->modules) assert(!state.enabled);
        modules::manager::disconnect();
        env->CallStaticVoidMethod(core, stop, token); checked();
        assert(!modules::manager::enable("sprint", true));
    }
    assert(vm->DestroyJavaVM() == JNI_OK);
    std::cout << "PASS real JNI manager bundle: shared parent + 2 child loaders, 36 modules, 4 targets, DTO and config transactions\n";
}
