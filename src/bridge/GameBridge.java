package net.onestep.hybrid;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;

/** Client-thread I/O for No Jump Delay. */
public final class GameBridge {
    private final Field instance, player, world, screen, jumpCooldown;
    private final Method onThread, alive;
    private long sequence, pending;
    private volatile boolean stopped;

    public GameBridge(ClassLoader loader, int target) throws ReflectiveOperationException {
        if (loader == null || (target != 12111 && target != 26100 && target != 26101 && target != 26102))
            throw new IllegalArgumentException("Exact supported target required");
        boolean named = target >= 26100;
        Class<?> client = Class.forName(named ? "net.minecraft.client.Minecraft" : "net.minecraft.class_310", false, loader);
        Class<?> p = Class.forName(named ? "net.minecraft.client.player.LocalPlayer" : "net.minecraft.class_746", false, loader);
        instance = field(client, named ? "instance" : "field_1700", client, true);
        player = field(client, named ? "player" : "field_1724", p, false);
        world = field(client, named ? "level" : "field_1687", Class.forName(named ? "net.minecraft.client.multiplayer.ClientLevel" : "net.minecraft.class_638", false, loader), false);
        screen = field(client, named ? "screen" : "field_1755", Class.forName(named ? "net.minecraft.client.gui.screens.Screen" : "net.minecraft.class_437", false, loader), false);
        jumpCooldown = field(p, named ? "noJumpDelay" : "field_6228", int.class, false);
        onThread = method(client, named ? "isSameThread" : "method_18854");
        alive = method(p, named ? "isAlive" : "method_5805");
    }
    private static Field field(Class<?> owner, String name, Class<?> type, boolean stat) throws ReflectiveOperationException {
        for (Class<?> c = owner; c != null; c = c.getSuperclass()) try {
            Field f = c.getDeclaredField(name);
            if (f.getType() != type || Modifier.isStatic(f.getModifiers()) != stat || !f.trySetAccessible()) throw new IllegalAccessException(name);
            return f;
        } catch (NoSuchFieldException missing) { }
        throw new NoSuchFieldException(name);
    }
    private static Method method(Class<?> owner, String name) throws ReflectiveOperationException {
        for (Class<?> c = owner; c != null; c = c.getSuperclass()) try {
            Method m = c.getDeclaredMethod(name);
            if (m.getReturnType() != boolean.class || Modifier.isStatic(m.getModifiers()) || !m.trySetAccessible()) throw new IllegalAccessException(name);
            return m;
        } catch (NoSuchMethodException missing) { }
        throw new NoSuchMethodException(name);
    }
    public Object[] capture(int feature) throws ReflectiveOperationException {
        if (stopped || feature != 1) return null;
        Object client = instance.get(null);
        if (client == null || !(boolean) onThread.invoke(client)) return null;
        pending = 0;
        Object p = player.get(client), w = world.get(client);
        if (p == null || w == null || screen.get(client) != null || !(boolean) alive.invoke(p)) return null;
        if (++sequence >= (1L << 53)) { stopped = true; return null; }
        pending = sequence;
        double[] data = new double[11];
        data[0] = 1; data[1] = 1; data[2] = 1; data[3] = jumpCooldown.getInt(p); data[10] = sequence;
        return new Object[]{this, p, w, data};
    }
    public boolean clearJump(Object[] packet) throws ReflectiveOperationException {
        if (stopped || packet == null || packet.length != 4 || packet[0] != this ||
            !(packet[3] instanceof double[] data) || data.length != 11 || data[0] != 1 || data[1] != 1 ||
            data[10] <= 0 || data[10] != pending || data[3] <= 0) return false;
        Object client = instance.get(null);
        if (client == null || !(boolean) onThread.invoke(client)) return false;
        pending = 0;
        if (player.get(client) != packet[1] || world.get(client) != packet[2] || screen.get(client) != null ||
            !(boolean) alive.invoke(packet[1]) || jumpCooldown.getInt(packet[1]) != data[3]) return false;
        jumpCooldown.setInt(packet[1], 0);
        return true;
    }
    public boolean tick() throws ReflectiveOperationException {
        var state = ManagerBridge.state().module("no_jump_delay");
        if (!state.enabled()) return false;
        Object[] packet = capture(1);
        return packet != null && ManagerBridge.current("no_jump_delay", state.generation()) && clearJump(packet);
    }
    public void stop() { stopped = true; pending = 0; }
}
