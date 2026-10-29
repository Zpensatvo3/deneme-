package net.onestep.visual;

import java.lang.reflect.Constructor;
import java.lang.reflect.Field;

/** Defined in Minecraft's loader so transformed FogRenderer can resolve it. No native methods. */
public final class SkyFogBridge {
    private static Constructor<?> vector;
    private static Field red, green, blue, alpha;
    private static volatile float[] config = {0, 0, 0};

    public static void prepare(ClassLoader loader) throws ReflectiveOperationException {
        Class<?> type = Class.forName("org.joml.Vector4f", true, loader);
        vector = type.getConstructor(float.class, float.class, float.class, float.class);
        red = type.getField("x"); green = type.getField("y");
        blue = type.getField("z"); alpha = type.getField("w");
    }
    public static void configure(int sky, int fog, float strength, boolean matchFog, boolean enabled) {
        float apply = enabled && matchFog ? 1 : 0;
        float[] previous = config;
        if (previous[0] == apply && previous[1] == fog && previous[2] == strength) return;
        config = new float[]{apply, fog, strength};
    }
    public static void stop() { config = new float[]{0, 0, 0}; }
    /** 26.1's computeFogColor owns and fills this output, rather than returning it. */
    public static void filterInto(Object output) {
        Object filtered = filter(output);
        if (filtered == output) return;
        try {
            red.setFloat(output, red.getFloat(filtered));
            green.setFloat(output, green.getFloat(filtered));
            blue.setFloat(output, blue.getFloat(filtered));
        } catch (ReflectiveOperationException ignored) { }
    }
    public static Object filter(Object original) {
        float[] values = config;
        if (values[0] == 0 || vector == null || original == null) return original;
        try {
            float strength = values[2] * .75f;
            int rgb = (int)values[1];
            return vector.newInstance(red.getFloat(original) * (1-strength) + ((rgb>>16)&255)/255f*strength,
                green.getFloat(original) * (1-strength) + ((rgb>>8)&255)/255f*strength,
                blue.getFloat(original) * (1-strength) + (rgb&255)/255f*strength, alpha.getFloat(original));
        } catch (ReflectiveOperationException failure) { return original; }
    }
}
