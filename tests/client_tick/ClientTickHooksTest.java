package net.onestep.tests;

import java.lang.reflect.*;
import java.util.concurrent.atomic.*;
import net.onestep.hybrid.ClientTickRelay;
import net.onestep.hybrid.ClientTickTransformer;
import org.objectweb.asm.*;

/** JVM verifier exercises for the shared tick transformer; no Minecraft needed. */
public final class ClientTickHooksTest {
    private static int checks;
    private static void check(boolean value,String message) { ++checks; if (!value) throw new AssertionError(message); }
    private static final class Loader extends ClassLoader {
        Loader() { super(ClientTickHooksTest.class.getClassLoader()); }
        Class<?> load(byte[] bytes) { return defineClass(null,bytes,0,bytes.length); }
    }
    private static byte[] fixture(String owner,String tick,String use,boolean includeTick,boolean staticTick) {
        ClassWriter w=new ClassWriter(ClassWriter.COMPUTE_FRAMES|ClassWriter.COMPUTE_MAXS);
        w.visit(Opcodes.V17,Opcodes.ACC_PUBLIC,owner,null,"java/lang/Object",null);
        for(String field:new String[]{"ticks","uses"}) w.visitField(Opcodes.ACC_PUBLIC,field,"I",null,null).visitEnd();
        MethodVisitor ctor=w.visitMethod(Opcodes.ACC_PUBLIC,"<init>","()V",null,null); ctor.visitCode();
        ctor.visitVarInsn(Opcodes.ALOAD,0); ctor.visitMethodInsn(Opcodes.INVOKESPECIAL,"java/lang/Object","<init>","()V",false);
        ctor.visitInsn(Opcodes.RETURN); ctor.visitMaxs(0,0); ctor.visitEnd();
        if(includeTick) body(w,owner,tick,"ticks",staticTick);
        body(w,owner,use,"uses",false); w.visitEnd(); return w.toByteArray();
    }
    private static void body(ClassWriter w,String owner,String method,String field,boolean stat) {
        MethodVisitor m=w.visitMethod(Opcodes.ACC_PUBLIC|(stat ? Opcodes.ACC_STATIC : 0),method,"()V",null,null); m.visitCode();
        if(!stat) {
            m.visitVarInsn(Opcodes.ALOAD,0); m.visitInsn(Opcodes.DUP); m.visitFieldInsn(Opcodes.GETFIELD,owner,field,"I");
            m.visitInsn(Opcodes.ICONST_1); m.visitInsn(Opcodes.IADD); m.visitFieldInsn(Opcodes.PUTFIELD,owner,field,"I");
            Label end=new Label(); m.visitVarInsn(Opcodes.ALOAD,0); m.visitFieldInsn(Opcodes.GETFIELD,owner,field,"I");
            m.visitJumpInsn(Opcodes.IFEQ,end); m.visitInsn(Opcodes.NOP); m.visitLabel(end);
        }
        m.visitInsn(Opcodes.RETURN); m.visitMaxs(0,0); m.visitEnd();
    }
    public static class NamedInventory {
        public int clicks;
        public NamedInventory(Object player) {}
        protected void slotClicked(Object slot,int id,int button,Object action) { ++clicks; }
        public boolean mouseClicked(Object click,boolean twice) { ++clicks; return true; }
        public boolean mouseClicked(double x,double y,int button) { ++clicks; return true; }
        public void clickSlot() { slotClicked(null,0,0,null); }
    }
    public static void main(String[] args) throws Exception {
        for(int target:new int[]{12111,26100,26101,26102}) {
            boolean named=target >= 26100;
            String owner=named ? "net/minecraft/client/Minecraft" : "net/minecraft/class_310";
            String tick=named ? "tick" : "method_1574",use=named ? "startUseItem" : "method_1583";
            byte[] original=fixture(owner,tick,use,true,false),modified=ClientTickTransformer.transform(original,target);
            Class<?> type=new Loader().load(modified); Object c=type.getConstructor().newInstance();
            AtomicInteger tickEvents=new AtomicInteger();
            Object token=new Object();
            ClientTickRelay.install(token,client -> {
                try { if(type.getField("ticks").getInt(client) != tickEvents.getAndIncrement()) throw new AssertionError("tick HEAD order"); }
                catch(ReflectiveOperationException error) { throw new AssertionError(error); }
            });
            type.getMethod(tick).invoke(c); type.getMethod(tick).invoke(c);
            check(tickEvents.get() == 2 && type.getField("ticks").getInt(c) == 2,"one event at tick HEAD for "+target);
            type.getMethod(use).invoke(c);
            check(type.getField("uses").getInt(c) == 1,"item use is not intercepted");
            ClientTickRelay.stop(new Object()); type.getMethod(tick).invoke(c);
            check(tickEvents.get() == 3,"stale owner cannot detach tick callback");
            ClientTickRelay.stop(token); type.getMethod(tick).invoke(c);
            check(tickEvents.get() == 3 && type.getField("ticks").getInt(c) == 4,"detached tick hook is harmless");
            check(type.getDeclaredFields().length == 2 && type.getDeclaredMethods().length == 2,"retransform preserves class schema");
            boolean rejected=false;
            try { ClientTickTransformer.transform(modified,target); } catch(IllegalArgumentException expected) { rejected=true; }
            check(rejected,"double injection rejected");
        }
        for(int kind=0;kind<4;++kind) {
            boolean rejected=false;
            try {
                byte[] bytes=fixture(kind == 0 ? "net/minecraft/Wrong" : "net/minecraft/class_310","method_1574","method_1583",kind != 1,kind == 2);
                ClientTickTransformer.transform(bytes,kind == 3 ? 99999 : 12111);
            } catch(IllegalArgumentException expected) { rejected=true; }
            check(rejected,"unexpected owner/method/access/version rejected "+kind);
        }
        byte[] screen=ClientTickTransformer.inventoryScreen(NamedInventory.class,Object.class,"slotClicked","mouseClicked");
        Class<?> fake=new Loader().load(screen);
        NamedInventory inventory=(NamedInventory)fake.getConstructor(Object.class).newInstance(new Object());
        check(!inventory.mouseClicked(new Object(),false),"named modern mouse click blocked");
        check(!inventory.mouseClicked(1.,2.,0),"named legacy mouse click blocked");
        inventory.clickSlot(); check(inventory.clicks==0,"named slot mutation callback blocked");
        boolean rejected=false;
        try { ClientTickTransformer.inventoryScreen(NamedInventory.class,Object.class,"missing","mouseClicked"); }
        catch(NoSuchMethodException expected) { rejected=true; }
        check(rejected,"fake inventory fails closed on missing callback mapping");
        System.out.println("PASS shared client tick transformer: "+checks+" checks ("+args[0]+"; verified JVM bytecode)");
    }
}
