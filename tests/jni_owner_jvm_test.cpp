// Optional integration test: starts a disposable JDK JVM, never Minecraft.
#include "../src/meta_jni.hpp"
#include <cassert>
#include <iostream>

namespace fixture
{
    BEGIN_KLASS_DEF(Object, "java/lang/Object")
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(List, "java/util/ArrayList")
        jni::constructor<> constructor{*this};
        jni::method<jint, "size"> size{*this};
        jni::field<jint, "size"> count{*this};
        jni::multi_method<jint, "missingMethod", "size"> fallback_size{*this};
        jni::multi_field<jint, "missingField", "size"> fallback_count{*this};
        jni::method<jboolean, "add", jni::NOT_STATIC, Object> add{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Map, "java/util/HashMap")
        jni::constructor<> constructor{*this};
        jni::method<jint, "size"> size{*this};
        jni::field<jint, "size"> count{*this};
        jni::multi_method<jint, "missingMethod", "size"> fallback_size{*this};
        jni::multi_field<jint, "missingField", "size"> fallback_count{*this};
        jni::method<Object, "put", jni::NOT_STATIC, Object, Object> put{*this};
    END_KLASS_DEF()
}
static_assert(std::is_same_v<decltype(fixture::List_members::size), decltype(fixture::Map_members::size)>);
static_assert(std::is_same_v<decltype(fixture::List_members::constructor), decltype(fixture::Map_members::constructor)>);

int main()
{
    JavaVM* vm = nullptr;
    JNIEnv* env = nullptr;
    JavaVMOption option{const_cast<char*>("-Xcheck:jni"), nullptr};
    JavaVMInitArgs options{}; options.version = JNI_VERSION_1_8; options.nOptions = 1; options.options = &option;
    assert(JNI_CreateJavaVM(&vm, reinterpret_cast<void**>(&env), &options) == JNI_OK);
    jni::init(); jni::set_thread_env(env);
    assert(jni::get_env() == env);
    {
        jni::frame frame{64};
        auto list = fixture::List::new_object(&fixture::List::constructor);
        auto map = fixture::Map::new_object(&fixture::Map::constructor);
        assert(!env->ExceptionCheck() && list && map);
        assert(list.is_instance_of<fixture::List>() && map.is_instance_of<fixture::Map>());
        fixture::Object value(env->NewStringUTF("test"));
        assert(list.add.call(value)); assert(!env->ExceptionCheck());
        map.put.call(value, value); assert(!env->ExceptionCheck());
        assert(list.add.call(value)); assert(!env->ExceptionCheck());
        assert(list.size.call() == 2); assert(!env->ExceptionCheck());
        assert(map.size.call() == 1); assert(!env->ExceptionCheck());
        assert(list.count.get() == 2 && map.count.get() == 1);
        assert(list.fallback_size.call() == 2); assert(!env->ExceptionCheck());
        assert(map.fallback_size.call() == 1); assert(!env->ExceptionCheck());
        assert(list.fallback_count.get() == 2 && map.fallback_count.get() == 1);
    }
    jni::shutdown();
    assert(vm->DestroyJavaVM() == JNI_OK);
    std::cout << "PASS -Xcheck:jni: unrelated classes safely share constructor/method/field signatures, including fallback bindings\n";
}
