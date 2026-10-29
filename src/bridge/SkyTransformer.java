package net.onestep.visual;

import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.Opcodes;
import org.objectweb.asm.tree.*;

/** Exact fog contracts: 1.21.11 returns a vector; 26.1 fills its last argument. */
public final class SkyTransformer {
    public static byte[] transform(byte[] original, int target) {
        if (target != 12111 && target != 26100 && target != 26101 && target != 26102)
            throw new IllegalArgumentException("Unsupported fog target");
        boolean named = target != 12111;
        ClassReader reader = new ClassReader(original);
        ClassNode node = new ClassNode(); reader.accept(node, 0);
        if (!node.name.equals(named ? "net/minecraft/client/renderer/fog/FogRenderer" : "net/minecraft/class_758"))
            throw new IllegalArgumentException("Wrong fog class for target " + target);
        String name = named ? "computeFogColor" : "method_62185";
        String descriptor = named ? "(Lnet/minecraft/client/Camera;FLnet/minecraft/client/multiplayer/ClientLevel;IFLorg/joml/Vector4f;)V"
            : "(Lnet/minecraft/class_4184;FLnet/minecraft/class_638;IF)Lorg/joml/Vector4f;";
        int changed = 0, matched = 0;
        for (MethodNode method : node.methods) {
            if (!method.name.equals(name) || !method.desc.equals(descriptor)) continue;
            if ((method.access & (Opcodes.ACC_STATIC | Opcodes.ACC_ABSTRACT | Opcodes.ACC_NATIVE)) != 0)
                throw new IllegalArgumentException("Unexpected fog method access");
            ++matched;
            for (AbstractInsnNode instruction : method.instructions.toArray()) {
                if (instruction.getOpcode() != (named ? Opcodes.RETURN : Opcodes.ARETURN)) continue;
                InsnList prefix = new InsnList();
                if (named) {
                    // Instance method: camera=1, partial tick=2, level=3,
                    // distance=4, darkness=5, caller-owned output vector=6.
                    prefix.add(new VarInsnNode(Opcodes.ALOAD, 6));
                    prefix.add(new MethodInsnNode(Opcodes.INVOKESTATIC, "net/onestep/visual/SkyFogBridge", "filterInto", "(Ljava/lang/Object;)V", false));
                } else {
                    prefix.add(new MethodInsnNode(Opcodes.INVOKESTATIC, "net/onestep/visual/SkyFogBridge", "filter", "(Ljava/lang/Object;)Ljava/lang/Object;", false));
                    prefix.add(new TypeInsnNode(Opcodes.CHECKCAST, "org/joml/Vector4f"));
                }
                method.instructions.insertBefore(instruction, prefix); ++changed;
            }
        }
        if (matched != 1 || changed == 0) throw new IllegalArgumentException("Fog return sites did not match target " + target);
        ClassWriter writer = new ClassWriter(reader, 0); node.accept(writer); return writer.toByteArray();
    }
}
