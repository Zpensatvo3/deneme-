package net.onestep.visual;

import java.lang.reflect.Field;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

/** No Minecraft compile-time dependency. Installed only for the binary's exact target. */
public final class SkyBridge implements InvocationHandler {
    private static volatile boolean active, shaderBlocked;
    private static Object iris;
    private static Method shaderCheck, matrixGet;
    private static Method projectionBuffer, shaderFog, modelView, sliceBuffer, sliceOffset, sliceLength, worldState;
    private static Field bufferId, skyState, skyColor;
    private static volatile float[] colors = {0, 0, 0, 0, 0};
    private final float[] projectionValues = new float[16], positionValues = new float[16];
    private final float[] fogValues = new float[5], fogUniform = new float[8];
    private static native void draw(float[] projection, float[] position, float[] fog);
    private static native boolean readUniform(int buffer, long offset, long available, float[] values);

    public static void install(ClassLoader loader, int target) throws ReflectiveOperationException {
        if (target != 12111
                && target != 26100 && target != 26101 && target != 26102)
            throw new IllegalArgumentException("Unsupported Sky target");
        boolean named = target == 26100 || target == 26101 || target == 26102;
        String api = "net.fabricmc.fabric.api.client.rendering.v1." + (named ? "level." : "world.");
        String events = named ? "LevelRenderEvents" : "WorldRenderEvents";
        Class<?> renderSystem = Class.forName("com.mojang.blaze3d.systems.RenderSystem", true, loader);
        Class<?> matrix = Class.forName("org.joml.Matrix4f", true, loader);
        matrixGet = matrix.getMethod("get", float[].class);
        projectionBuffer = renderSystem.getMethod("getProjectionMatrixBuffer");
        modelView = renderSystem.getMethod("getModelViewMatrix");
        Class<?> context = Class.forName(api + (named ? "AbstractLevelRenderContext" : "AbstractWorldRenderContext"), true, loader);
        worldState = context.getMethod(named ? "levelState" : "worldState");
        skyState = Class.forName(named ? "net.minecraft.client.renderer.state.level.LevelRenderState" : "net.minecraft.class_11658", true, loader)
            .getField(named ? "skyRenderState" : "field_63087");
        skyColor = Class.forName(named ? "net.minecraft.client.renderer.state.level.SkyRenderState" : "net.minecraft.class_12076", true, loader)
            .getField(named ? "skyColor" : "field_63097");
        shaderFog = renderSystem.getMethod("getShaderFog");
        Class<?> slice = Class.forName("com.mojang.blaze3d.buffers.GpuBufferSlice", true, loader);
        sliceBuffer = slice.getMethod("buffer");
        sliceOffset = slice.getMethod("offset"); sliceLength = slice.getMethod("length");
        bufferId = Class.forName(named ? "com.mojang.blaze3d.opengl.GlBuffer" : "net.minecraft.class_10859", true, loader)
            .getDeclaredField(named ? "handle" : "field_57842");
        bufferId.setAccessible(true);
        try {
            Class<?> irisApi = Class.forName("net.irisshaders.iris.api.v0.IrisApi", false, loader);
            iris = irisApi.getMethod("getInstance").invoke(null);
            shaderCheck = irisApi.getMethod("isShaderPackInUse");
        } catch (ClassNotFoundException ignored) { }
        SkyBridge handler = new SkyBridge();
        // 26.1 removed BEFORE_ENTITIES. START_MAIN runs after sky, before opaque
        // terrain, with the world projection/fog buffers bound and OpenGL allowed.
        register(loader, api, events, named ? "START_MAIN" : "BEFORE_ENTITIES", named ? "StartMain" : "BeforeEntities", handler);
        register(loader, api, events, "END_EXTRACTION", "EndExtraction", handler);
        active = true;
    }

    private static void register(ClassLoader loader, String api, String eventsName, String field, String listener, SkyBridge handler)
            throws ReflectiveOperationException {
        Class<?> events = Class.forName(api + eventsName, true, loader);
        Class<?> type = Class.forName(api + eventsName + "$" + listener, true, loader);
        Object proxy = Proxy.newProxyInstance(SkyBridge.class.getClassLoader(), new Class<?>[]{type}, handler);
        Object event = events.getField(field).get(null);
        Class.forName("net.fabricmc.fabric.api.event.Event", true, loader).getMethod("register", Object.class).invoke(event, proxy);
    }

    public static void configureColors(int sky, int fog, float strength, boolean matchFog, boolean enabled) {
        float[] previous = colors;
        if (previous[0] == (enabled ? 1 : 0) && previous[1] == sky && previous[2] == fog
                && previous[3] == strength && previous[4] == (matchFog ? 1 : 0)) return;
        colors = new float[]{enabled ? 1 : 0, sky, fog, strength, matchFog ? 1 : 0};
    }
    public static void stop() { active = false; colors = new float[]{0, 0, 0, 0, 0}; }
    public static boolean shaderBlocked() { return shaderBlocked; }
    public static boolean healthy() { return active; }

    private boolean readSlice(Object slice, float[] values) throws ReflectiveOperationException {
        if (slice == null) return false;
        Object buffer = sliceBuffer.invoke(slice);
        return buffer != null && readUniform(bufferId.getInt(buffer),
            ((Number)sliceOffset.invoke(slice)).longValue(), ((Number)sliceLength.invoke(slice)).longValue(), values);
    }
    private static int blend(int original, int target, float strength) {
        int result = 0;
        for (int shift : new int[]{16, 8, 0}) {
            int channel = Math.round(((original>>shift)&255)*(1-strength) + ((target>>shift)&255)*strength);
            result |= Math.max(0, Math.min(255, channel)) << shift;
        }
        return original & 0xff000000 | result;
    }

    @Override public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
        if (method.getDeclaringClass() == Object.class) {
            if (method.getName().equals("hashCode")) return System.identityHashCode(proxy);
            if (method.getName().equals("equals")) return proxy == args[0];
            return "OnestepSky";
        }
        if (!active || args == null || args.length != 1) return null;
        try {
            shaderBlocked = shaderCheck != null && Boolean.TRUE.equals(shaderCheck.invoke(iris));
            if (shaderBlocked) return null;
            if (method.getName().equals("endExtraction")) {
                float[] config = colors;
                if (config[0] != 0) {
                    Object state = skyState.get(worldState.invoke(args[0]));
                    skyColor.setInt(state, blend(skyColor.getInt(state), (int)config[1], config[3]));
                }
                return null;
            }
            if (!readSlice(projectionBuffer.invoke(null), projectionValues)) return null;
            matrixGet.invoke(modelView.invoke(null), (Object)positionValues);
            if (!readSlice(shaderFog.invoke(null), fogUniform)) return null;
            System.arraycopy(fogUniform, 0, fogValues, 0, 3);
            fogValues[3] = Math.min(fogUniform[4], fogUniform[6]);
            fogValues[4] = Math.min(fogUniform[5], fogUniform[7]);
            draw(projectionValues, positionValues, fogValues);
        } catch (Throwable failure) {
            active = false;
        }
        return null;
    }
}
