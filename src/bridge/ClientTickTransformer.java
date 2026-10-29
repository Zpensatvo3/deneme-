package net.onestep.hybrid;

import org.objectweb.asm.*;
import org.objectweb.asm.tree.*;

/** Shared client tick hook and Auto Totem inventory screen. */
public final class ClientTickTransformer {
    /** Generate the local, non-clickable inventory screen used by Auto Totem.
     * Reflect the mapped virtual method descriptors so both mouse API generations work.
     */
    public static byte[] inventoryScreen(Class<?> inventory,Class<?> player,String slotClick,String mouseClick) throws ReflectiveOperationException {
        inventory.getConstructor(player);
        String parent=Type.getInternalName(inventory), owner="net/onestep/hybrid/TotemScreen";
        ClassWriter writer=new ClassWriter(ClassWriter.COMPUTE_MAXS);
        writer.visit(Opcodes.V17,Opcodes.ACC_PUBLIC|Opcodes.ACC_FINAL,owner,null,parent,null);
        String ctor=Type.getMethodDescriptor(Type.VOID_TYPE,Type.getType(player));
        MethodVisitor init=writer.visitMethod(Opcodes.ACC_PUBLIC,"<init>",ctor,null,null);
        init.visitCode(); init.visitVarInsn(Opcodes.ALOAD,0); init.visitVarInsn(Opcodes.ALOAD,1);
        init.visitMethodInsn(Opcodes.INVOKESPECIAL,parent,"<init>",ctor,false);
        init.visitInsn(Opcodes.RETURN); init.visitMaxs(0,0); init.visitEnd();
        java.util.Set<String> overridden=new java.util.HashSet<>(); boolean slot=false, mouse=false;
        for(Class<?> type=inventory;type!=null;type=type.getSuperclass()) for(java.lang.reflect.Method m:type.getDeclaredMethods()) {
            boolean isSlot=m.getName().equals(slotClick), isMouse=m.getName().equals(mouseClick);
            if (!isSlot && !isMouse) continue;
            int access=m.getModifiers();
            if(java.lang.reflect.Modifier.isStatic(access) || java.lang.reflect.Modifier.isPrivate(access) ||
                java.lang.reflect.Modifier.isFinal(access) ||
                (!java.lang.reflect.Modifier.isPublic(access) && !java.lang.reflect.Modifier.isProtected(access))) continue;
            if ((isSlot && m.getReturnType()!=void.class) || (isMouse && m.getReturnType()!=boolean.class))
                throw new IllegalArgumentException("Unexpected inventory mouse signature");
            String desc=Type.getMethodDescriptor(m);
            if(!overridden.add(m.getName()+desc)) continue;
            MethodVisitor method=writer.visitMethod(Opcodes.ACC_PUBLIC,m.getName(),desc,null,null);
            method.visitCode(); if(isMouse) method.visitInsn(Opcodes.ICONST_0);
            method.visitInsn(isMouse ? Opcodes.IRETURN : Opcodes.RETURN); method.visitMaxs(0,0); method.visitEnd();
            slot|=isSlot; mouse|=isMouse;
        }
        if(!slot || !mouse) throw new NoSuchMethodException("Inventory mouse callbacks");
        writer.visitEnd(); return writer.toByteArray();
    }

    public static byte[] transform(byte[] original, int target) {
        if (target != 12111 && target != 26100 && target != 26101 && target != 26102) throw new IllegalArgumentException("Client tick target");
        boolean named = target >= 26100;
        String owner = named ? "net/minecraft/client/Minecraft" : "net/minecraft/class_310";
        String tick = named ? "tick" : "method_1574";
        ClassReader reader = new ClassReader(original); ClassNode node = new ClassNode(); reader.accept(node, 0);
        if (!node.name.equals(owner)) throw new IllegalArgumentException("Client tick owner");
        int ticks = 0;
        for (MethodNode method : node.methods) {
            if (!method.desc.equals("()V") || !method.name.equals(tick)) continue;
            if ((method.access & (Opcodes.ACC_STATIC | Opcodes.ACC_NATIVE | Opcodes.ACC_ABSTRACT)) != 0) throw new IllegalArgumentException("Client tick access");
            for (AbstractInsnNode insn : method.instructions)
                if (insn instanceof MethodInsnNode call && call.owner.equals("net/onestep/hybrid/ClientTickRelay")) throw new IllegalArgumentException("Client tick already hooked");
            InsnList prefix = new InsnList(); prefix.add(new VarInsnNode(Opcodes.ALOAD, 0));
            prefix.add(new MethodInsnNode(Opcodes.INVOKESTATIC,"net/onestep/hybrid/ClientTickRelay","onTick","(Ljava/lang/Object;)V",false));
            method.instructions.insert(prefix); ++ticks;
        }
        if (ticks != 1) throw new IllegalArgumentException("Client tick entry count");
        ClassWriter writer = new ClassWriter(reader,ClassWriter.COMPUTE_MAXS); node.accept(writer); return writer.toByteArray();
    }
}
