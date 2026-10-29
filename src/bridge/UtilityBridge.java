package net.onestep.hybrid;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;

/** Original OneStep adapter for utility modules. No hooks, native callbacks or queued writes. */
public final class UtilityBridge {
    public static final int FAST_PLACE = 1, SPRINT = 2, FULL_BRIGHT = 3;
    private final ClassLoader loader;
    private final Class<?> playerType, optionsType;
    private final Field instance, player, world, screen, options;
    private final Method onThread, alive;
    private final boolean[] prepared = new boolean[4];
    private Field useKey, useCooldown, sprintKey, gammaValue;
    private Method pressed, mainHand, offHand, empty, item;
    private Method canSprint, sprinting, setSprinting, usingItem, getGamma;
    private Class<?> blockItem, potionItem;
    private Object sprintPlayer, sprintWorld, gammaOwner;
    private double originalGamma;
    private volatile boolean stopped;

    public UtilityBridge(ClassLoader loader, int target) throws ReflectiveOperationException {
        if (loader == null || target != 12111)
            throw new IllegalArgumentException("UtilityBridge requires exact Fabric 1.21.11");
        this.loader = loader;
        Class<?> client = load("class_310");
        playerType = load("class_746"); optionsType = load("class_315");
        instance = field(client, "field_1700", client, true);
        player = field(client, "field_1724", playerType, false);
        world = field(client, "field_1687", load("class_638"), false);
        screen = field(client, "field_1755", load("class_437"), false);
        options = field(client, "field_1690", optionsType, false);
        onThread = method(client, "method_18854", boolean.class);
        alive = method(playerType, "method_5805", boolean.class);
    }

    private Class<?> load(String name) throws ClassNotFoundException {
        return Class.forName("net.minecraft." + name, false, loader);
    }
    private static Field field(Class<?> owner, String name, Class<?> type, boolean isStatic)
            throws ReflectiveOperationException {
        for (Class<?> c = owner; c != null; c = c.getSuperclass()) {
            try {
                Field f = c.getDeclaredField(name);
                if (f.getType() != type || Modifier.isStatic(f.getModifiers()) != isStatic || !f.trySetAccessible())
                    throw new IllegalAccessException("UtilityBridge field mismatch: " + name);
                return f;
            } catch (NoSuchFieldException missing) { /* Continue through actual inheritance only. */ }
        }
        throw new NoSuchFieldException(name);
    }
    private static Method method(Class<?> owner, String name, Class<?> result, Class<?>... arguments)
            throws ReflectiveOperationException {
        for (Class<?> c = owner; c != null; c = c.getSuperclass()) {
            try {
                Method m = c.getDeclaredMethod(name, arguments);
                if (m.getReturnType() != result || Modifier.isStatic(m.getModifiers()) || !m.trySetAccessible())
                    throw new IllegalAccessException("UtilityBridge method mismatch: " + name);
                return m;
            } catch (NoSuchMethodException missing) { /* Continue through actual inheritance only. */ }
        }
        throw new NoSuchMethodException(name);
    }
    private static boolean yes(Method method, Object value) throws ReflectiveOperationException {
        return value != null && (boolean) method.invoke(value);
    }
    private static void validate(int feature) {
        if (feature < FAST_PLACE || feature > FULL_BRIGHT) throw new IllegalArgumentException("Unknown utility feature");
    }

    /** Validate each capability separately; a missing item binding cannot disable Full Bright. */
    public void prepare(int feature) throws ReflectiveOperationException {
        validate(feature);
        if (stopped) throw new IllegalStateException("UtilityBridge stopped");
        if (prepared[feature]) return;
        if (feature == FAST_PLACE) {
            Class<?> binding = load("class_304"), stack = load("class_1799");
            useKey = field(optionsType, "field_1904", binding, false);
            useCooldown = field(instance.getType(), "field_1752", int.class, false);
            pressed = method(binding, "method_1434", boolean.class);
            mainHand = method(playerType, "method_6047", stack);
            offHand = method(playerType, "method_6079", stack);
            empty = method(stack, "method_7960", boolean.class);
            item = method(stack, "method_7909", load("class_1792"));
            blockItem = load("class_1747"); potionItem = load("class_1812");
        } else if (feature == SPRINT) {
            Class<?> binding = load("class_304");
            sprintKey = field(optionsType, "field_1867", binding, false);
            pressed = method(binding, "method_1434", boolean.class);
            canSprint = method(playerType, "method_48300", boolean.class);
            sprinting = method(playerType, "method_5624", boolean.class);
            setSprinting = method(playerType, "method_5728", void.class, boolean.class);
            usingItem = method(playerType, "method_6115", boolean.class);
        } else {
            Class<?> option = load("class_7172");
            getGamma = method(optionsType, "method_42473", option);
            gammaValue = field(option, "field_37868", Object.class, false);
        }
        prepared[feature] = true;
    }

    private Object clientOnThread() throws ReflectiveOperationException {
        Object client = instance.get(null);
        return client != null && yes(onThread, client) ? client : null;
    }

    public void update(int feature, boolean active, int value, boolean physicalUseHeld)
            throws ReflectiveOperationException {
        validate(feature);
        if (stopped) return;
        if (!active) { release(feature); return; }
        prepare(feature);
        Object client = clientOnThread();
        if (client == null) return;
        if (feature == FULL_BRIGHT) { brighten(client); return; }
        Object p = player.get(client), w = world.get(client);
        if (p == null || w == null || screen.get(client) != null || !yes(alive, p)) {
            release(feature); return;
        }
        if (feature == FAST_PLACE) fastPlace(client, p, value, physicalUseHeld);
        else sprint(client, p, w, value != 0);
    }

    private boolean supported(Object stack) throws ReflectiveOperationException {
        if (stack == null || yes(empty, stack)) return false;
        Object heldItem = item.invoke(stack);
        return blockItem.isInstance(heldItem) || potionItem.isInstance(heldItem);
    }
    private void fastPlace(Object client, Object p, int delay, boolean physicalUseHeld)
            throws ReflectiveOperationException {
        Object opts = options.get(client);
        if (opts == null) return;
        Object key = useKey.get(opts);
        if (key == null || (!yes(pressed, key) && !physicalUseHeld)) return;
        if (!supported(mainHand.invoke(p)) && !supported(offHand.invoke(p))) return;
        int desired = Math.max(0, Math.min(4, delay));
        if (useCooldown.getInt(client) > desired) useCooldown.setInt(client, desired);
        // No extra item-use, synthetic click, drinking-time or server-cooldown bypass.
    }
    private void sprint(Object client, Object p, Object w, boolean stopWhileUsing)
            throws ReflectiveOperationException {
        if (sprintPlayer != p || sprintWorld != w) { sprintPlayer = sprintWorld = null; }
        Object opts = options.get(client);
        if (opts == null) return;
        Object key = sprintKey.get(opts);
        if (key == null) return;
        // A real sprint binding takes over ownership; never release that hold.
        if (yes(pressed, key)) { sprintPlayer = sprintWorld = null; return; }
        if (!yes(sprinting, p)) { sprintPlayer = sprintWorld = null; }
        if ((stopWhileUsing && yes(usingItem, p)) || !yes(canSprint, p)) return;
        if (!yes(sprinting, p)) {
            // Record ownership before invoking a mod-overridable mutator.
            sprintPlayer = p; sprintWorld = w;
            setSprinting.invoke(p, true);
        }
    }
    private void brighten(Object client) throws ReflectiveOperationException {
        Object opts = options.get(client), gamma = opts == null ? null : getGamma.invoke(opts);
        if (gamma == null) return;
        Object value = gammaValue.get(gamma);
        if (!(value instanceof Double current) || !Double.isFinite(current)) return;
        if (gammaOwner != gamma) { gammaOwner = gamma; originalGamma = current; }
        else if (current != 16.0) originalGamma = current; // Preserve a later manual preference.
        gammaValue.set(gamma, Double.valueOf(16.0));
    }

    /** Cleanup is allowed during menus/focus loss/auth shutdown, but only on the game thread. */
    public boolean release(int feature) throws ReflectiveOperationException {
        validate(feature);
        if (feature == FAST_PLACE || !prepared[feature]) return true;
        if (feature == SPRINT && sprintPlayer == null) return true;
        if (feature == FULL_BRIGHT && gammaOwner == null) return true;
        Object client = clientOnThread();
        if (client == null) return false;
        if (feature == SPRINT) {
            Object p = player.get(client), w = world.get(client), opts = options.get(client);
            if (p == sprintPlayer && w == sprintWorld && yes(alive, p) && yes(sprinting, p)) {
                Object key = opts == null ? null : sprintKey.get(opts);
                if (key == null) return false;
                if (!yes(pressed, key)) setSprinting.invoke(p, false);
            }
            sprintPlayer = sprintWorld = null;
        } else {
            Object opts = options.get(client), gamma = opts == null ? null : getGamma.invoke(opts);
            // Restore only the current option and only while our value still owns it.
            if (gamma == gammaOwner && Double.valueOf(16.0).equals(gammaValue.get(gamma)))
                gammaValue.set(gamma, Double.valueOf(originalGamma));
            gammaOwner = null;
        }
        return true;
    }

    /** Renderer cleanup must happen first; this method never touches game objects. */
    public void stop() {
        stopped = true;
        sprintPlayer = sprintWorld = gammaOwner = null;
    }
}
