package net.onestep.hybrid;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;

/** Client-thread Fast EXP controller; decisions, timing and game I/O stay in Java. */
public final class ItemBridge {
    public static final int FAST_EXP = 2;
    private final ClassLoader loader;
    private final boolean named;
    private final Class<?> clientType, playerType;
    private final Field instance, player, world, screen;
    private final Method onThread, alive;
    private final boolean[] prepared = new boolean[5];
    private Field expItem, useCooldown;
    private Method empty;
    private Method mainHand, offHand, getItem, useItem;
    private long sequence, expSequence;
    private long nextUseNanos;
    private Object sessionPlayer, sessionWorld;
    private boolean armed, blocked;
    private volatile boolean stopped;

    public ItemBridge(ClassLoader loader, int target) throws ReflectiveOperationException {
        if (loader == null || (target != 12111 && target != 26100 && target != 26101 && target != 26102))
            throw new IllegalArgumentException("ItemBridge requires an exact supported target");
        this.loader = loader;
        named = target >= 26100;
        clientType = load("class_310"); playerType = load("class_746");
        instance = field(clientType, "field_1700", clientType, true);
        player = field(clientType, "field_1724", playerType, false);
        world = field(clientType, "field_1687", load("class_638"), false);
        screen = field(clientType, "field_1755", load("class_437"), false);
        onThread = method(clientType, "method_18854", boolean.class);
        alive = method(playerType, "method_5805", boolean.class);
    }
    private Class<?> load(String name) throws ClassNotFoundException {
        String mapped = named ? switch (name) {
            case "class_310" -> "net.minecraft.client.Minecraft";
            case "class_746" -> "net.minecraft.client.player.LocalPlayer";
            case "class_638" -> "net.minecraft.client.multiplayer.ClientLevel";
            case "class_437" -> "net.minecraft.client.gui.screens.Screen";
            case "class_1799" -> "net.minecraft.world.item.ItemStack";
            case "class_1792" -> "net.minecraft.world.item.Item";
            case "class_1802" -> "net.minecraft.world.item.Items";
            default -> throw new IllegalArgumentException("Unsupported item class: " + name);
        } : "net.minecraft." + name;
        return Class.forName(mapped, false, loader);
    }
    private String mapped(String name) {
        if (!named) return name;
        return switch (name) {
            case "field_1700" -> "instance";
            case "field_1724" -> "player";
            case "field_1687" -> "level";
            case "field_1755" -> "screen";
            case "field_8287" -> "EXPERIENCE_BOTTLE";
            case "field_1752" -> "rightClickDelay";
            case "method_18854" -> "isSameThread";
            case "method_5805" -> "isAlive";
            case "method_7960" -> "isEmpty";
            case "method_6047" -> "getMainHandItem";
            case "method_6079" -> "getOffhandItem";
            case "method_7909" -> "getItem";
            case "method_1583" -> "startUseItem";
            default -> throw new IllegalArgumentException("Unsupported item binding: " + name);
        };
    }
    private Field field(Class<?> owner, String name, Class<?> type, boolean isStatic)
            throws ReflectiveOperationException {
        name = mapped(name);
        for (Class<?> c = owner; c != null; c = c.getSuperclass()) {
            try {
                Field f = c.getDeclaredField(name);
                if (f.getType() != type || Modifier.isStatic(f.getModifiers()) != isStatic || !f.trySetAccessible())
                    throw new IllegalAccessException("ItemBridge field mismatch: " + name);
                return f;
            } catch (NoSuchFieldException missing) { /* Exact inherited member only. */ }
        }
        throw new NoSuchFieldException(name);
    }
    private Method method(Class<?> owner, String name, Class<?> result, Class<?>... arguments)
            throws ReflectiveOperationException {
        name = mapped(name);
        for (Class<?> c = owner; c != null; c = c.getSuperclass()) {
            try {
                Method m = c.getDeclaredMethod(name, arguments);
                if (m.getReturnType() != result || Modifier.isStatic(m.getModifiers()) || !m.trySetAccessible())
                    throw new IllegalAccessException("ItemBridge method mismatch: " + name);
                return m;
            } catch (NoSuchMethodException missing) { /* Exact inherited member only. */ }
        }
        throw new NoSuchMethodException(name);
    }
    private static boolean yes(Method method, Object object) throws ReflectiveOperationException {
        return object != null && (boolean) method.invoke(object);
    }
    public void prepare(int feature) throws ReflectiveOperationException {
        if (stopped || feature != FAST_EXP) throw new IllegalArgumentException("Invalid item feature/lifecycle");
        if (prepared[feature]) return;
        Class<?> stack = load("class_1799");
        empty = method(stack, "method_7960", boolean.class);

            mainHand = method(playerType, "method_6047", stack);
            offHand = method(playerType, "method_6079", stack);
            getItem = method(stack, "method_7909", load("class_1792"));
            expItem = field(load("class_1802"), "field_8287", load("class_1792"), true);
            useCooldown = field(clientType, "field_1752", int.class, false);
            useItem = method(clientType, "method_1583", void.class);
        prepared[feature] = true;
    }
    private Object clientOnThread() throws ReflectiveOperationException {
        if (stopped) return null;
        Object client = instance.get(null);
        return client != null && yes(onThread, client) ? client : null;
    }
    private boolean valid(Object client, Object p, Object w) throws ReflectiveOperationException {
        return p != null && w != null && screen.get(client) == null && yes(alive, p);
    }
    private long nextSequence() {
        if (++sequence >= (1L << 53)) { stopped = true; return 0; }
        return sequence;
    }

    private double[] consume(Object[] packet, int size, int dataIndex, int length)
            throws ReflectiveOperationException {
        if (clientOnThread() == null || packet == null || packet.length != size || packet[0] != this ||
            !(packet[dataIndex] instanceof double[] values) || values.length != length || values[0] != 1) return null;
        int sequenceIndex = 1;
        if (values[sequenceIndex] <= 0 || values[sequenceIndex] != expSequence) return null;
        expSequence = 0;
        return values;
    }
    /** {owner, player, world, mainStack, offStack, double[4]: schema/sequence/mainEXP/offEXP}. */
    public Object[] captureExp() throws ReflectiveOperationException {
        Object client = clientOnThread();
        if (client == null) return null;
        expSequence = 0;
        prepare(FAST_EXP);
        Object p = player.get(client), w = world.get(client);
        if (!valid(client, p, w)) return null;
        Object mainStack = mainHand.invoke(p), offStack = offHand.invoke(p), exp = expItem.get(null);
        if (mainStack == null || offStack == null || exp == null) return null;
        boolean mainExp = !yes(empty, mainStack) && getItem.invoke(mainStack) == exp;
        boolean offExp = !yes(empty, offStack) && getItem.invoke(offStack) == exp;
        double[] values = {1, expSequence = nextSequence(), mainExp ? 1 : 0, offExp ? 1 : 0};
        return stopped ? null : new Object[]{this, p, w, mainStack, offStack, values};
    }
    public boolean useExp(Object[] packet, boolean physicalRight) throws ReflectiveOperationException {
        double[] before = consume(packet, 6, 5, 4);
        if (before == null || !physicalRight || (before[2] != 1 && before[3] != 1)) return false;
        Object[] now = captureExp();
        expSequence = 0;
        if (now == null || now[1] != packet[1] || now[2] != packet[2] || now[3] != packet[3] || now[4] != packet[4])
            return false;
        double[] current = (double[]) now[5];
        if (before[2] != current[2] || before[3] != current[3]) return false;
        Object client = clientOnThread();
        if (client == null) return false;
        useCooldown.setInt(client, 0);
        useItem.invoke(client); // Exactly one request; no second SendInput click.
        return true;
    }
    private void resetSession() {
        armed = false; nextUseNanos = 0; sessionPlayer = sessionWorld = null; expSequence = 0;
    }
    public void update(boolean enabled, boolean permitted, boolean held, int minCps, int maxCps)
            throws ReflectiveOperationException {
        if (clientOnThread() == null) return;
        if (!enabled || !held) { blocked = false; resetSession(); return; }
        if (!permitted) { resetSession(); return; }
        if (blocked) return;
        Object[] observed = captureExp();
        if (observed == null) { resetSession(); return; }
        if (sessionPlayer != observed[1] || sessionWorld != observed[2]) {
            resetSession(); sessionPlayer = observed[1]; sessionWorld = observed[2];
            observed = captureExp();
            if (observed == null) { resetSession(); return; }
        }
        double[] flags = (double[]) observed[5];
        if (flags[2] != 1 && flags[3] != 1) { blocked = true; resetSession(); return; }
        long now = System.nanoTime();
        if (armed && now - nextUseNanos < 0) return;
        try {
            if (!useExp(observed, true)) { blocked = true; resetSession(); return; }
        } catch (ReflectiveOperationException | RuntimeException failure) {
            blocked = true; resetSession(); throw failure;
        }
        int low = Math.max(1, Math.min(50, Math.min(minCps, maxCps)));
        int high = Math.max(low, Math.min(50, Math.max(minCps, maxCps)));
        java.util.concurrent.ThreadLocalRandom random = java.util.concurrent.ThreadLocalRandom.current();
        int cps = random.nextInt(low, high + 1);
        long intervalMs = Math.max(10, Math.round(1000.0 / cps + random.nextDouble(-1.5, 1.5)));
        nextUseNanos = now + intervalMs * 1_000_000L;
        armed = true;
    }
    public void stop() {
        stopped = true; blocked = false; resetSession();
    }
}
