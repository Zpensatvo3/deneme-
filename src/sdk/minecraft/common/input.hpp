#pragma once
// Shared SDK layout; target-specific JNI differences are selected at compile time.
#include "client_entities.hpp"

// Shared Fabric bindings with compile-time target profiles.
namespace maps
{
        BEGIN_KLASS_DEF(SimpleOption, ONESTEP_MC_NAME("net/minecraft/class_7172"))
        jni::field<Object, ONESTEP_MC_NAME("field_37868")> value{ *this };
    jni::method<Object, ONESTEP_MC_NAME("method_41753")> getValue{ *this };
	jni::method<void, ONESTEP_MC_NAME("method_41748"), jni::NOT_STATIC, Object> setValue{ *this };
    END_KLASS_DEF()

    BEGIN_KLASS_DEF(InputKeyType, ONESTEP_MC_NAME("net/minecraft/class_3675$class_307"))
        jni::field<InputKeyType, ONESTEP_MC_NAME("field_1668"), jni::STATIC> KEYSYM{ *this };
        jni::field<InputKeyType, ONESTEP_MC_NAME("field_1671"), jni::STATIC> SCANCODE{ *this };
        jni::field<InputKeyType, ONESTEP_MC_NAME("field_1672"), jni::STATIC> MOUSE{ *this };
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(InputKey, ONESTEP_MC_NAME("net/minecraft/class_3675$class_306"))
        jni::method<jint, ONESTEP_MC_NAME("method_1444")> getCode{ *this };
        jni::method<InputKeyType, ONESTEP_MC_NAME("method_1442")> getCategory{ *this };
    END_KLASS_DEF()
        BEGIN_KLASS_DEF(KeyBinding, ONESTEP_MC_NAME("net/minecraft/class_304"))
    jni::method<void, ONESTEP_MC_NAME("method_1437"), jni::STATIC> unpressAll{ *this };
    jni::field<InputKey, ONESTEP_MC_NAME("field_1655")> boundKey{ *this };
        jni::method<jboolean, ONESTEP_MC_NAME("method_1434")> isPressed{ *this };
    jni::method<void, ONESTEP_MC_NAME("method_23481"), jni::NOT_STATIC, jboolean> setPressed{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(GameOptions, ONESTEP_MC_NAME("net/minecraft/class_315"))
        jni::field<SimpleOption, ONESTEP_MC_NAME("field_1843")> mouseSensitivity{ *this };
    jni::field<SimpleOption, ONESTEP_MC_NAME("field_1840")> gammaSetting{ *this };
    jni::field<KeyBinding, ONESTEP_MC_NAME("field_1886")> attackKey{ *this };
    jni::field<KeyBinding, ONESTEP_MC_NAME("field_1904")> useKey{ *this };
    jni::field<KeyBinding, ONESTEP_MC_NAME("field_1903")> jumpKey{ *this };
	jni::field<KeyBinding, ONESTEP_MC_NAME("field_1894")> forwardKey{ *this };
	jni::field<KeyBinding, ONESTEP_MC_NAME("field_1881")> backKey{ *this };
	jni::field<KeyBinding, ONESTEP_MC_NAME("field_1913")> leftKey{ *this };
	jni::field<KeyBinding, ONESTEP_MC_NAME("field_1849")> rightKey{ *this };
	jni::field<KeyBinding, ONESTEP_MC_NAME("field_1832")> sneakKey{ *this };
    jni::field<SimpleOption, ONESTEP_MC_NAME("field_1826")> fov{ *this };
    jni::method<SimpleOption, ONESTEP_MC_NAME("method_42495")> getMouseSensitivity{ *this };
    jni::method<SimpleOption, ONESTEP_MC_NAME("method_42473")> getGamma{ *this };
    END_KLASS_DEF()

    BEGIN_KLASS_DEF(MouseInput, ONESTEP_MC_NAME("net/minecraft/class_11910"))
        jni::constructor<jint, jint> constructor{*this};
    END_KLASS_DEF()
        BEGIN_KLASS_DEF(Mouse, ONESTEP_MC_NAME("net/minecraft/class_312"))
    jni::method<jboolean, ONESTEP_MC_NAME("method_1613")> isCursorLocked{ *this };
    jni::method<void, ONESTEP_MC_NAME("method_1610")> unlockCursor{ *this };
    jni::method<void, ONESTEP_MC_NAME("method_1612")> lockCursor{ *this };
        jni::method<void, ONESTEP_MC_NAME("method_1601"), jni::NOT_STATIC, jlong, MouseInput, jint> onMouseInput{*this};
        struct mouse_button
        {
            const Mouse_members& owner;
            bool is_valid() const { return owner.onMouseInput.is_valid(); }
            void call(jlong window, jint button, jint action, jint modifiers) const
            {
                auto input = MouseInput::new_object(&MouseInput::constructor, button, modifiers);
                if (input) owner.onMouseInput.call(window, input, action);
            }
        } onMouseButton{*this};
    jni::field<jboolean, ONESTEP_MC_NAME("field_1791")> leftButtonClicked{ *this };
    jni::field<jboolean, ONESTEP_MC_NAME("field_1788")> rightButtonClicked{ *this };
        jni::field<jdouble, ONESTEP_MC_NAME("field_1789")> cursorDeltaX{ *this };
    jni::field<jdouble, ONESTEP_MC_NAME("field_1787")> cursorDeltaY{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(RenderTickCounter, ONESTEP_MC_NAME("net/minecraft/class_9779$class_9781"))
        jni::field<jfloat, ONESTEP_MC_NAME("field_51959")> tickDelta{ *this };
        jni::method<jfloat, ONESTEP_MC_NAME("method_60637"), jni::NOT_STATIC, jboolean> getTickDelta{ *this };

        float get_tick_delta()
        {
            JNIEnv* env = jni::get_env();
            if (getTickDelta.is_valid())
            {
                float d = getTickDelta.call(JNI_FALSE);
                bool exc = env && env->ExceptionCheck();
                if (exc) env->ExceptionClear();
                if (!exc && std::isfinite(d) && d >= 0.0f && d <= 2.0f)
                    return d;
            }
            if (tickDelta.is_valid())
            {
                float d = tickDelta.get();
                bool exc = env && env->ExceptionCheck();
                if (exc) env->ExceptionClear();
                if (!exc && std::isfinite(d) && d >= 0.0f && d <= 2.0f)
                    return d;
            }
            return 1.0f;
        }
    END_KLASS_DEF()

}
