#pragma once
// Shared SDK layout; target-specific JNI differences are selected at compile time.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <string>
#include "../../../meta_jni.hpp"
#include "../../../platform/adapter/bindings_profile.hpp"
#include "../../../maths/maths.hpp"

// Shared Fabric bindings with compile-time target profiles.
namespace maps
{
    using bindings_profile = platform::adapter::bindings::selected;
    BEGIN_KLASS_DEF(Object, "java/lang/Object")
        END_KLASS_DEF()

        BEGIN_KLASS_DEF(Comparable, "java/lang/Comparable")
        END_KLASS_DEF()

        BEGIN_KLASS_DEF_EX(Integer, "java/lang/Integer", Comparable)
        jni::method<jint, "intValue", jni::NOT_STATIC> intValue{ *this };
        END_KLASS_DEF()

        BEGIN_KLASS_DEF(Class, "java/lang/Class")
        operator jclass() const
    {
        return (jclass)object_instance;
    }
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(String, "java/lang/String")
        static String create(const char* str)
    {
        return String(jni::get_env()->NewStringUTF(str));
    }

    std::string to_string() const
    {
        if (!object_instance) return std::string();
        jstring str_obj = (jstring)object_instance;
        jsize utf8_size = jni::get_env()->GetStringUTFLength(str_obj);
        jsize size = jni::get_env()->GetStringLength(str_obj);

        std::string str(utf8_size, '\0');
        jni::get_env()->GetStringUTFRegion(str_obj, 0, size, str.data());
        return str;
    }
    END_KLASS_DEF()

    BEGIN_KLASS_DEF(UUID, "java/util/UUID")
        jni::method<String, "toString"> toString{ *this };
    END_KLASS_DEF()

    BEGIN_KLASS_DEF(Optional, "java/util/Optional")
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(Collection, "java/util/Collection")
        jni::method<jni::array<Object>, "toArray"> toArray{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF_EX(List, "java/util/List", Collection)
        jni::method<Object, "get", jni::NOT_STATIC, jint> get{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF_EX(DefaultedList, ONESTEP_MC_NAME("net/minecraft/class_2371"), List)
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(Iterator, "java/util/Iterator")
        jni::method<jboolean, "hasNext"> hasNext{ *this };
        jni::method<Object, "next"> next{ *this };
        END_KLASS_DEF()

	BEGIN_KLASS_DEF(Iterable, "java/lang/Iterable")
		jni::method<Iterator, "iterator"> iterator{ *this };
	END_KLASS_DEF()

    BEGIN_KLASS_DEF(Map, "java/util/Map")
        jni::method<Collection, "values"> values{ *this };
    END_KLASS_DEF()

    BEGIN_KLASS_DEF(AtomicReferenceArray, "java/util/concurrent/atomic/AtomicReferenceArray")
        jni::method<jint, "length"> length{ *this };
        jni::method<Object, "get", jni::NOT_STATIC, jint> get{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(URL, "java/net/URL")
        jni::constructor<String> constructor{ *this };
    jni::method<String, "toString"> toString{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(ClassLoader, "java/lang/ClassLoader")
        jni::method<Class, "findClass", jni::NOT_STATIC, String> findClass{ *this };
    jni::method<Class, "loadClass", jni::NOT_STATIC, String> loadClass{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF_EX(URLClassLoader, "java/net/URLClassLoader", ClassLoader)
        jni::constructor<jni::array<URL>> constructor{ *this };
    jni::constructor<jni::array<URL>, ClassLoader> constructor2{ *this };
    jni::method<void, "addURL", jni::NOT_STATIC, URL> addURL{ *this };
    jni::method<jni::array<URL>, "getURLs", jni::NOT_STATIC> getURLs{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(Double, "java/lang/Double")
        jni::method<jdouble, "doubleValue"> doubleValue{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(PrintStream, "java/io/PrintStream")
        jni::method<void, "println", jni::NOT_STATIC, String> println{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(System, "java/lang/System")
        jni::field<PrintStream, "out", jni::STATIC> out{ *this };
    END_KLASS_DEF()

}
