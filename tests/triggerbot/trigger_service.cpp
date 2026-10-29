#include <jni.h>

// Offline replacement for the native coordinator only. The Java gameplay
// controller and its reflection bindings execute unchanged.
extern "C" JNIEXPORT jlong JNICALL
Java_net_onestep_hybrid_WorldBridge_triggerService(JNIEnv* env, jobject,
    jint operation, jobject player, jobject world, jobject entity, jint value, jlong epoch)
{
    jclass fixture = env->FindClass("net/minecraft/TriggerbotBridgeTest");
    if (!fixture) return 0;
    jmethodID service = env->GetStaticMethodID(fixture, "service", "(ILjava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;IJ)J");
    if (!service) { env->DeleteLocalRef(fixture); return 0; }
    jlong result = env->CallStaticLongMethod(fixture, service, operation, player, world, entity, value, epoch);
    env->DeleteLocalRef(fixture);
    return result;
}
