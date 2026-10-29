import java.nio.file.Path;
import java.util.Set;
import java.util.zip.ZipFile;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import org.objectweb.asm.*;
import org.objectweb.asm.tree.*;
import net.onestep.visual.SkyBridge;

/** Real public signatures with inert bodies and a recording Event. No Minecraft,
 * OpenGL or JNI code is executed. This exercises the shipping bridge's reflection,
 * event registration, extraction dispatch, color update and stop lifecycle. */
final class SkyBridgeInstallationTest {
    private static final String EVENT = "net/fabricmc/fabric/api/event/Event";

    private static void constructor(ClassWriter writer) {
        MethodVisitor method = writer.visitMethod(Opcodes.ACC_PUBLIC, "<init>", "()V", null, null);
        method.visitCode(); method.visitVarInsn(Opcodes.ALOAD, 0);
        method.visitMethodInsn(Opcodes.INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
        method.visitInsn(Opcodes.RETURN); method.visitMaxs(0, 0); method.visitEnd();
    }
    private static byte[] objectFixture(byte[] original, Set<String> fields, Set<String> methods) {
        ClassNode type = BridgeCompatibilityTest.node(original);
        ClassWriter writer = new ClassWriter(ClassWriter.COMPUTE_MAXS);
        writer.visit(type.version, Opcodes.ACC_PUBLIC, type.name, null, "java/lang/Object", null);
        constructor(writer);
        int fieldCount = 0;
        for (FieldNode field : type.fields) {
            if (!fields.contains(field.name)) continue;
            // Fixtures need initialized values supplied by the test, not vanilla static initialization.
            writer.visitField(field.access & ~Opcodes.ACC_FINAL, field.name, field.desc, null, null).visitEnd(); ++fieldCount;
        }
        if (fieldCount != fields.size()) throw new AssertionError("Missing fixture field in " + type.name);
        for (String name : methods) {
            MethodNode source = type.methods.stream().filter(m -> m.name.equals(name)).findFirst().orElseThrow();
            MethodVisitor method = writer.visitMethod(source.access & ~(Opcodes.ACC_ABSTRACT | Opcodes.ACC_NATIVE), source.name, source.desc, null, null);
            method.visitCode();
            Type result = Type.getReturnType(source.desc);
            switch (result.getSort()) {
                case Type.VOID -> method.visitInsn(Opcodes.RETURN);
                case Type.OBJECT, Type.ARRAY -> { method.visitInsn(Opcodes.ACONST_NULL); method.visitInsn(Opcodes.ARETURN); }
                case Type.LONG -> { method.visitInsn(Opcodes.LCONST_0); method.visitInsn(Opcodes.LRETURN); }
                case Type.FLOAT -> { method.visitInsn(Opcodes.FCONST_0); method.visitInsn(Opcodes.FRETURN); }
                case Type.DOUBLE -> { method.visitInsn(Opcodes.DCONST_0); method.visitInsn(Opcodes.DRETURN); }
                default -> { method.visitInsn(Opcodes.ICONST_0); method.visitInsn(Opcodes.IRETURN); }
            }
            method.visitMaxs(0, 0); method.visitEnd();
        }
        writer.visitEnd(); return writer.toByteArray();
    }
    private static byte[] recordingEvent() {
        ClassWriter writer = new ClassWriter(ClassWriter.COMPUTE_MAXS);
        writer.visit(Opcodes.V17, Opcodes.ACC_PUBLIC, EVENT, null, "java/lang/Object", null);
        constructor(writer);
        writer.visitField(Opcodes.ACC_PUBLIC, "listener", "Ljava/lang/Object;", null, null).visitEnd();
        writer.visitField(Opcodes.ACC_PUBLIC, "registrations", "I", null, null).visitEnd();
        MethodVisitor method = writer.visitMethod(Opcodes.ACC_PUBLIC, "register", "(Ljava/lang/Object;)V", null, null);
        method.visitCode(); method.visitVarInsn(Opcodes.ALOAD, 0); method.visitVarInsn(Opcodes.ALOAD, 1);
        method.visitFieldInsn(Opcodes.PUTFIELD, EVENT, "listener", "Ljava/lang/Object;");
        method.visitVarInsn(Opcodes.ALOAD, 0); method.visitInsn(Opcodes.DUP);
        method.visitFieldInsn(Opcodes.GETFIELD, EVENT, "registrations", "I"); method.visitInsn(Opcodes.ICONST_1);
        method.visitInsn(Opcodes.IADD); method.visitFieldInsn(Opcodes.PUTFIELD, EVENT, "registrations", "I");
        method.visitInsn(Opcodes.RETURN); method.visitMaxs(0, 0); method.visitEnd(); writer.visitEnd();
        return writer.toByteArray();
    }
    static void check(BridgeCompatibilityTest reader, ZipFile client, Path renderingJar, int target) throws Exception {
        boolean named = BridgeCompatibilityTest.namedTarget(target);
        var loader = new BridgeCompatibilityTest.FixtureLoader();
        Set<String> renderMethods = Set.of("getShaderFog", "getProjectionMatrixBuffer", "getModelViewMatrix");
        {
            loader.define(objectFixture(reader.read(client, "com/mojang/blaze3d/buffers/GpuBuffer"), Set.of(), Set.of()));
            loader.define(objectFixture(reader.read(client, "com/mojang/blaze3d/buffers/GpuBufferSlice"), Set.of(), Set.of("buffer", "offset", "length")));
            loader.define(objectFixture(reader.read(client, named ? "com/mojang/blaze3d/opengl/GlBuffer" : "net/minecraft/class_10859"),
                Set.of(named ? "handle" : "field_57842"), Set.of()));
        }
        loader.define(objectFixture(reader.read(client, "com/mojang/blaze3d/systems/RenderSystem"), Set.of(), renderMethods));
        String api = "net/fabricmc/fabric/api/client/rendering/v1/" + (named ? "level/" : "world/");
        String eventsName = api + (named ? "LevelRenderEvents" : "WorldRenderEvents");
        String eventField = named ? "START_MAIN" : "BEFORE_ENTITIES";
        Class<?> abstractContext = null, extractionContext = null, sky = null;
        Object world = null;
        String skyField = named ? "skyRenderState" : "field_63087", colorField = named ? "skyColor" : "field_63097";
        try (ZipFile fabric = new ZipFile(renderingJar.toFile())) {
            var fabricReader = new BridgeCompatibilityTest(null);
            {
                String stateName = named ? "net/minecraft/client/renderer/state/level/LevelRenderState" : "net/minecraft/class_11658";
                String skyName = named ? "net/minecraft/client/renderer/state/level/SkyRenderState" : "net/minecraft/class_12076";
                sky = loader.define(objectFixture(reader.read(client, skyName), Set.of(colorField), Set.of()));
                Class<?> level = loader.define(objectFixture(reader.read(client, stateName), Set.of(skyField), Set.of()));
                world = level.getConstructor().newInstance();
                level.getField(skyField).set(world, sky.getConstructor().newInstance());
                abstractContext = loader.define(BridgeCompatibilityTest.interfaceFixture(fabricReader.read(fabric,
                    api + (named ? "AbstractLevelRenderContext" : "AbstractWorldRenderContext")), Set.of("()L" + stateName + ";")));
            }
            for (String context : named ? new String[]{"LevelTerrainRenderContext", "LevelExtractionContext"} : new String[]{"WorldRenderContext", "WorldExtractionContext"}) {
                byte[] source = fabricReader.read(fabric, api + context);
                Class<?> type = loader.define(BridgeCompatibilityTest.interfaceFixture(source,
                    Set.of()));
                if (context.endsWith("ExtractionContext")) extractionContext = type;
            }
            Class<?> eventType = loader.define(recordingEvent());
            Set<String> eventFields = Set.of(eventField, "END_EXTRACTION");
            Class<?> events = loader.define(objectFixture(fabricReader.read(fabric, eventsName), eventFields, Set.of()));
            String listener = named ? "StartMain" : "BeforeEntities";
            String context = named ? "LevelTerrainRenderContext" : "WorldRenderContext";
            loader.define(BridgeCompatibilityTest.interfaceFixture(fabricReader.read(fabric, eventsName + "$" + listener), Set.of("(L" + api + context + ";)V")));
            loader.define(BridgeCompatibilityTest.interfaceFixture(fabricReader.read(fabric, eventsName + "$EndExtraction"),
                Set.of("(L" + api + (named ? "LevelExtractionContext" : "WorldExtractionContext") + ";)V")));
            for (String field : eventFields) events.getField(field).set(null, eventType.getConstructor().newInstance());
            byte[] bridge;
            try (var stream = SkyBridge.class.getResourceAsStream("SkyBridge.class")) { bridge = stream.readAllBytes(); }
            Class<?> bridgeType = loader.define(bridge);
            bridgeType.getMethod("install", ClassLoader.class, int.class).invoke(null, loader, target);
            Method healthy = bridgeType.getMethod("healthy");
            if (!Boolean.TRUE.equals(healthy.invoke(null))) throw new AssertionError("Sky did not install");
            for (String field : eventFields) {
                Object event = events.getField(field).get(null);
                if (eventType.getField("listener").get(event) == null || eventType.getField("registrations").getInt(event) != 1)
                    throw new AssertionError("Sky registered wrong event");
            }
            if (target >= 12111) {
                Object state = world;
                Object contextObject = Proxy.newProxyInstance(loader, new Class<?>[]{abstractContext, extractionContext}, (p, m, a) -> state);
                Object event = events.getField("END_EXTRACTION").get(null), callback = eventType.getField("listener").get(event);
                Method extract = callback.getClass().getMethod("endExtraction", extractionContext);
                Object skyObject = world.getClass().getField(skyField).get(world);
                sky.getField(colorField).setInt(skyObject, 0xff000000);
                bridgeType.getMethod("configureColors", int.class, int.class, float.class, boolean.class, boolean.class)
                    .invoke(null, 0xffffff, 0, .5f, true, true);
                extract.invoke(callback, contextObject);
                if (sky.getField(colorField).getInt(skyObject) != 0xff808080 || !Boolean.TRUE.equals(healthy.invoke(null)))
                    throw new AssertionError("Sky extraction dispatch/color blending failed");
                bridgeType.getMethod("stop").invoke(null);
                sky.getField(colorField).setInt(skyObject, 0x123456);
                extract.invoke(callback, contextObject);
                if (sky.getField(colorField).getInt(skyObject) != 0x123456) throw new AssertionError("Stopped Sky callback still changed color");
            }
            if (!Boolean.FALSE.equals(healthy.invoke(null))) throw new AssertionError("Sky failed to stop");
        }
    }
}
