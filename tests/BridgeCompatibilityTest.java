import java.nio.charset.StandardCharsets;
import java.nio.file.Path;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;
import java.util.HashSet;
import java.util.Optional;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.zip.ZipFile;
import org.objectweb.asm.*;
import org.objectweb.asm.commons.ClassRemapper;
import org.objectweb.asm.commons.Remapper;
import org.objectweb.asm.tree.*;
import org.objectweb.asm.tree.analysis.*;
import org.joml.Vector4f;
import net.onestep.visual.SkyTransformer;
import net.onestep.visual.SkyFogBridge;
import net.onestep.visual.SkyBridge;

/** Inspects local, unmodified Mojang jars; never starts or attaches to Minecraft. */
public final class BridgeCompatibilityTest {
    static boolean namedTarget(int target) { return target == 26100 || target == 26101 || target == 26102; }
    private final Map<String, String> classes = new HashMap<>(), methods = new HashMap<>(), fields = new HashMap<>();
    private final Map<String, String> official = new HashMap<>();

    BridgeCompatibilityTest(Path mapping) throws Exception {
        if (mapping == null) return; // 26.1 ships Mojang names, with no remapping.
        try (ZipFile jar = new ZipFile(mapping.toFile())) {
            String owner = "";
            String text = new String(jar.getInputStream(jar.getEntry("mappings/mappings.tiny")).readAllBytes(), StandardCharsets.UTF_8);
            for (String line : text.split("\n")) {
                String[] parts = line.split("\t");
                if (parts[0].equals("c")) {
                    owner = parts[1]; classes.put(owner, parts[2]); official.put(parts[2], owner);
                } else if (parts.length >= 5 && parts[0].isEmpty()) {
                    String key = owner + "|" + parts[3] + "|" + parts[2];
                    if (parts[1].equals("m")) methods.put(key, parts[4]);
                    if (parts[1].equals("f")) fields.put(key, parts[4]);
                }
            }
        }
    }
    byte[] read(ZipFile jar, String type) throws Exception {
        String name = official.getOrDefault(type, type);
        byte[] original = jar.getInputStream(jar.getEntry(name + ".class")).readAllBytes();
        ClassWriter output = new ClassWriter(0);
        new ClassReader(original).accept(new ClassRemapper(output, new Remapper(Opcodes.ASM9) {
            @Override public String map(String name) { return classes.getOrDefault(name, name); }
            @Override public String mapMethodName(String owner, String name, String desc) {
                return methods.getOrDefault(owner + "|" + name + "|" + desc, name);
            }
            @Override public String mapFieldName(String owner, String name, String desc) {
                return fields.getOrDefault(owner + "|" + name + "|" + desc, name);
            }
        }), 0);
        return output.toByteArray();
    }
    static ClassNode node(byte[] bytes) {
        ClassNode result = new ClassNode(); new ClassReader(bytes).accept(result, 0); return result;
    }
    private static long calls(byte[] bytes, String owner, String name) {
        long result = 0;
        for (MethodNode method : node(bytes).methods)
            for (AbstractInsnNode instruction : method.instructions)
                if (instruction instanceof MethodInsnNode call && call.owner.equals(owner) && call.name.equals(name)) ++result;
        return result;
    }
    private static void method(byte[] bytes, String name) {
        if (node(bytes).methods.stream().noneMatch(method -> method.name.equals(name)))
            throw new AssertionError("Missing reflected method " + name);
    }
    private static void field(byte[] bytes, String name) {
        if (node(bytes).fields.stream().noneMatch(field -> field.name.equals(name)))
            throw new AssertionError("Missing reflected field " + name);
    }
    private static void contract(byte[] bytes, String name, String descriptor, boolean isStatic) {
        ClassNode type = node(bytes);
        boolean method = descriptor.startsWith("(");
        int flags = method ? type.methods.stream().filter(m -> m.name.equals(name) && m.desc.equals(descriptor))
                .map(m -> m.access).findFirst().orElseThrow(() -> new AssertionError("Missing method " + type.name + "." + name + descriptor))
            : type.fields.stream().filter(f -> f.name.equals(name) && f.desc.equals(descriptor))
                .map(f -> f.access).findFirst().orElseThrow(() -> new AssertionError("Missing field " + type.name + "." + name + descriptor));
        if (((flags & Opcodes.ACC_STATIC) != 0) != isStatic) throw new AssertionError("Wrong static flag " + type.name + "." + name);
    }
    private static void verify(byte[] bytes) throws Exception {
        ClassNode type = node(bytes);
        for (MethodNode method : type.methods) {
            if ((method.access & (Opcodes.ACC_ABSTRACT | Opcodes.ACC_NATIVE)) == 0)
                new Analyzer<BasicValue>(new BasicVerifier()).analyze(type.name, method);
        }
    }
    private static void fogBehavior() throws Exception {
        SkyFogBridge.prepare(BridgeCompatibilityTest.class.getClassLoader());
        Vector4f original = new Vector4f(.2f, .3f, .4f, .8f);
        SkyFogBridge.configure(0, 0xff0000, 1, true, true);
        Vector4f filtered = (Vector4f)SkyFogBridge.filter(original);
        if (Math.abs(filtered.x - .8f) > .0001f || Math.abs(filtered.y - .075f) > .0001f || filtered.w != original.w)
            throw new AssertionError("Fog blending or alpha changed");
        if (original.x != .2f || filtered == original) throw new AssertionError("Shared vanilla fog vector was mutated");
        SkyFogBridge.configure(0, 0xff0000, 1, false, true);
        if (SkyFogBridge.filter(original) != original) throw new AssertionError("Disabled fog matching changed fog");
        SkyFogBridge.stop();
        if (SkyFogBridge.filter(original) != original || SkyFogBridge.filter(null) != null)
            throw new AssertionError("Stopped bridge must pass through");
        SkyFogBridge.configure(0, 0xff0000, 1, true, true);
        SkyFogBridge.filterInto(original);
        if (Math.abs(original.x - .8f) > .0001f || Math.abs(original.y - .075f) > .0001f || original.w != .8f)
            throw new AssertionError("26.1 output-vector fog contract changed");
        SkyFogBridge.stop();
        Vector4f before = new Vector4f(original);
        SkyFogBridge.filterInto(original); SkyFogBridge.filterInto(null);
        if (!original.equals(before)) throw new AssertionError("Stopped output-vector filter changed fog");
    }
    /** Execute the transformed fog method on a small JVM fixture, without game code.
     * The real method above is separately inspected and its transformed body analyzed. */
    private static void fogExecution(byte[] actual, int target) throws Exception {
        ClassNode source = node(actual);
        MethodNode method = source.methods.stream().filter(m -> m.name.equals(namedTarget(target) ? "computeFogColor" : "method_62185"))
            .findFirst().orElseThrow();
        ClassWriter fixture = new ClassWriter(ClassWriter.COMPUTE_MAXS);
        fixture.visit(source.version, Opcodes.ACC_PUBLIC, source.name, null, "java/lang/Object", null);
        MethodVisitor init = fixture.visitMethod(Opcodes.ACC_PUBLIC, "<init>", "()V", null, null);
        init.visitCode(); init.visitVarInsn(Opcodes.ALOAD, 0);
        init.visitMethodInsn(Opcodes.INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
        init.visitInsn(Opcodes.RETURN); init.visitMaxs(0, 0); init.visitEnd();
        MethodVisitor body = fixture.visitMethod(Opcodes.ACC_PUBLIC, method.name, method.desc, null, null);
        body.visitCode();
        // Retain the actual argument layout and classfile version; only replace vanilla's calculation.
        if (namedTarget(target)) body.visitVarInsn(Opcodes.ALOAD, 6);
        else { body.visitTypeInsn(Opcodes.NEW, "org/joml/Vector4f"); body.visitInsn(Opcodes.DUP);
            body.visitMethodInsn(Opcodes.INVOKESPECIAL, "org/joml/Vector4f", "<init>", "()V", false); }
        for (float value : new float[]{.2f, .3f, .4f, .8f}) body.visitLdcInsn(value);
        body.visitMethodInsn(Opcodes.INVOKEVIRTUAL, "org/joml/Vector4f", "set", "(FFFF)Lorg/joml/Vector4f;", false);
        if (namedTarget(target)) { body.visitInsn(Opcodes.POP); body.visitInsn(Opcodes.RETURN); }
        else body.visitInsn(Opcodes.ARETURN);
        body.visitMaxs(0, 0); body.visitEnd(); fixture.visitEnd();
        FixtureLoader loader = new FixtureLoader();
        for (Type arg : Type.getArgumentTypes(method.desc)) {
            if (arg.getSort() != Type.OBJECT || !arg.getInternalName().startsWith("net/minecraft/")) continue;
            ClassWriter empty = new ClassWriter(0);
            empty.visit(source.version, Opcodes.ACC_PUBLIC, arg.getInternalName(), null, "java/lang/Object", null); empty.visitEnd();
            loader.define(empty.toByteArray());
        }
        Class<?> type = loader.define(SkyTransformer.transform(fixture.toByteArray(), target));
        Method call = type.getDeclaredMethods()[0];
        Object instance = type.getConstructor().newInstance();
        SkyFogBridge.configure(0, 0xff0000, 1, true, true);
        Vector4f out = new Vector4f();
        Object result = namedTarget(target) ? call.invoke(instance, null, 0f, null, 0, 0f, out)
            : call.invoke(instance, null, 0f, null, 0, 0f);
        Vector4f color = namedTarget(target) ? out : (Vector4f)result;
        if (Math.abs(color.x - .8f) > .0001f || Math.abs(color.y - .075f) > .0001f || color.w != .8f)
            throw new AssertionError("Transformed fog method failed on JVM");
        SkyFogBridge.stop();
    }

    static final class FixtureLoader extends ClassLoader {
        FixtureLoader() { super(BridgeCompatibilityTest.class.getClassLoader()); }
        Class<?> define(byte[] bytes) { return defineClass(null, bytes, 0, bytes.length); }
    }
    static byte[] interfaceFixture(byte[] original, Set<String> descriptors) {
        ClassNode type = node(original);
        ClassWriter writer = new ClassWriter(0);
        writer.visit(type.version, Opcodes.ACC_PUBLIC | Opcodes.ACC_ABSTRACT | Opcodes.ACC_INTERFACE,
            type.name, null, "java/lang/Object", null);
        Set<String> found = new HashSet<>();
        for (MethodNode method : type.methods) {
            if (!descriptors.contains(method.desc) || (method.access & Opcodes.ACC_PUBLIC) == 0
                    || (method.access & Opcodes.ACC_STATIC) != 0) continue;
            writer.visitMethod(Opcodes.ACC_PUBLIC | Opcodes.ACC_ABSTRACT, method.name, method.desc, null, null).visitEnd(); found.add(method.desc);
        }
        if (!found.equals(descriptors)) throw new AssertionError("Interface contract changed: " + type.name);
        writer.visitEnd(); return writer.toByteArray();
    }
    /** Real interface signatures, synthetic style/initializers: no game class is executed. */
    private static void uniformLayout(ZipFile client) throws Exception {
        String fog = new String(client.getInputStream(client.getEntry("assets/minecraft/shaders/include/fog.glsl")).readAllBytes(), StandardCharsets.UTF_8).replaceAll("\\s+", "");
        if (!fog.contains("uniformFog{vec4FogColor;floatFogEnvironmentalStart;floatFogEnvironmentalEnd;floatFogRenderDistanceStart;floatFogRenderDistanceEnd;"))
            throw new AssertionError("Fog uniform offsets changed");
        String projection = new String(client.getInputStream(client.getEntry("assets/minecraft/shaders/include/projection.glsl")).readAllBytes(), StandardCharsets.UTF_8).replaceAll("\\s+", "");
        if (!projection.contains("uniformProjection{mat4ProjMat;")) throw new AssertionError("Projection uniform layout changed");
    }
    private static void mouseOrder(byte[] bytes) {
        int checked = 0;
        for (MethodNode method : node(bytes).methods) {
            if (!method.name.equals("<init>") || !method.desc.equals("(II)V")) continue;
            for (AbstractInsnNode instruction : method.instructions) {
                if (!(instruction instanceof FieldInsnNode field) || field.getOpcode() != Opcodes.PUTFIELD) continue;
                int expected = field.name.equals("comp_4801") ? 1 : field.name.equals("comp_4797") ? 2 : 0;
                if (expected == 0) continue;
                AbstractInsnNode load = field.getPrevious();
                while (load != null && load.getOpcode() < 0) load = load.getPrevious();
                if (!(load instanceof VarInsnNode variable) || variable.getOpcode() != Opcodes.ILOAD || variable.var != expected)
                    throw new AssertionError("MouseInput constructor argument order changed");
                ++checked;
            }
        }
        if (checked != 2) throw new AssertionError("MouseInput button/modifiers fields did not match");
    }
    private static void fabricContracts(Path path, int target) throws Exception {
        boolean named = namedTarget(target);
        String api = "net/fabricmc/fabric/api/client/rendering/v1/" + (named ? "level/" : "world/");
        String events = api + (named ? "LevelRenderEvents" : "WorldRenderEvents");
        BridgeCompatibilityTest reader = new BridgeCompatibilityTest(null);
        try (ZipFile fabric = new ZipFile(path.toFile())) {
            String event = named ? "START_MAIN" : "BEFORE_ENTITIES", listener = named ? "StartMain" : "BeforeEntities";
            contract(reader.read(fabric, events), event, "Lnet/fabricmc/fabric/api/event/Event;", true);
            contract(reader.read(fabric, events + "$" + listener), named ? "startMain" : "beforeEntities",
                "(L" + api + (named ? "LevelTerrainRenderContext" : "WorldRenderContext") + ";)V", false);
            {
                contract(reader.read(fabric, events), "END_EXTRACTION", "Lnet/fabricmc/fabric/api/event/Event;", true);
                contract(reader.read(fabric, events + "$EndExtraction"), "endExtraction",
                    "(L" + api + (named ? "LevelExtractionContext" : "WorldExtractionContext") + ";)V", false);
                contract(reader.read(fabric, api + (named ? "AbstractLevelRenderContext" : "AbstractWorldRenderContext")),
                    named ? "levelState" : "worldState", named ? "()Lnet/minecraft/client/renderer/state/level/LevelRenderState;"
                    : "()Lnet/minecraft/class_11658;", false);
            }
        }
    }
    public static void main(String[] args) throws Exception {
        if (args.length < 3 || args.length > 4) throw new IllegalArgumentException("client.jar intermediary-v2.jar-or-- target-id [fabric-rendering.jar]");
        int target = Integer.parseInt(args[2]);
        boolean named = namedTarget(target);
        BridgeCompatibilityTest test = new BridgeCompatibilityTest(named ? null : Path.of(args[1]));
        try (ZipFile client = new ZipFile(args[0])) {
            byte[] render = test.read(client, "com/mojang/blaze3d/systems/RenderSystem");
            {
                method(render, "getShaderFog");
                byte[] slice = test.read(client, "com/mojang/blaze3d/buffers/GpuBufferSlice");
                method(slice, "buffer"); method(slice, "offset"); method(slice, "length");
                contract(test.read(client, named ? "com/mojang/blaze3d/opengl/GlBuffer" : "net/minecraft/class_10859"),
                    named ? "handle" : "field_57842", "I", false);
                uniformLayout(client);
            }
            {
                if (node(test.read(client, named ? "net/minecraft/client/renderer/item/ItemStackRenderState" : "net/minecraft/class_10444")).methods.stream()
                        .noneMatch(method -> method.name.equals("<init>") && method.desc.equals("()V")))
                    throw new AssertionError("ItemRenderState default constructor missing");
            }
            if (target >= 12111) {
                if (!named) mouseOrder(test.read(client, "net/minecraft/class_11910"));
                method(render, "getProjectionMatrixBuffer"); method(render, "getModelViewMatrix");
                String levelState = named ? "net/minecraft/client/renderer/state/level/LevelRenderState" : "net/minecraft/class_11658";
                String skyState = named ? "net/minecraft/client/renderer/state/level/SkyRenderState" : "net/minecraft/class_12076";
                contract(test.read(client, levelState), named ? "skyRenderState" : "field_63087", "L" + skyState + ";", false);
                contract(test.read(client, skyState), named ? "skyColor" : "field_63097", "I", false);
                byte[] fog = test.read(client, named ? "net/minecraft/client/renderer/fog/FogRenderer" : "net/minecraft/class_758");
                byte[] transformed = SkyTransformer.transform(fog, target);
                if (calls(transformed, "net/onestep/visual/SkyFogBridge", named ? "filterInto" : "filter") == 0)
                    throw new AssertionError("No fog return sites transformed");
                for (int unsupported : new int[]{0, 26103, 26200}) {
                    try { SkyTransformer.transform(fog, unsupported); throw new AssertionError("Unknown fog target accepted"); }
                    catch (IllegalArgumentException expectedFailure) { }
                }
                try { SkyTransformer.transform(fog, named ? 12111 : 26100); throw new AssertionError("Wrong fog namespace accepted"); }
                catch (IllegalArgumentException expectedFailure) { }
                verify(transformed);
                fogBehavior();
                fogExecution(fog, target);
            }
            if (args.length == 4) {
                fabricContracts(Path.of(args[3]), target);
                SkyBridgeInstallationTest.check(test, client, Path.of(args[3]), target);
            }
            System.out.println("PASS target " + target + ": real fog contracts, ASM analysis, isolated JVM fog behavior"
                + (args.length == 4 ? ", Fabric render events" : " (Fabric event jar not provided)"));
        }
    }
}
