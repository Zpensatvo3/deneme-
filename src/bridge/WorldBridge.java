package net.onestep.hybrid;

import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.lang.reflect.Proxy;
import java.util.List;
import java.util.HashMap;
import java.util.Map;

/** OneStep world I/O and Java-owned combat and Auto Totem controllers. */
public class WorldBridge {
    private final ClassLoader loader;
    private final int target;
    private final Map<String, String> names;
    private final Class<?> clientType, playerType;
    private final Field instance, player, world, screen, options, manager;
    private final Method onThread, alive;
    private final boolean[] prepared = new boolean[8];
    private Field selected, main, pearlItem, cooldown;
    private Method empty, item, count, copy, same;
    private Method inventory, sync, use, usingItem;
    private Object[] pearlOwner;
    private long sequence, pearlNonce;
    private volatile boolean stopped;

    public WorldBridge(ClassLoader loader, int target) throws ReflectiveOperationException {
        this(loader, target, null);
    }
    public WorldBridge(ClassLoader loader, int target, String[] bindings) throws ReflectiveOperationException {
        if (loader == null || (target != 12111 &&
            target != 26100 && target != 26101 && target != 26102) || (bindings == null && target != 12111))
            throw new IllegalArgumentException("WorldBridge requires an exact supported target and bindings");
        this.target = target;
        names = new HashMap<>();
        if (bindings != null) {
            if (bindings.length == 0 || (bindings.length & 1) != 0) throw new IllegalArgumentException("World binding pairs");
            for (int i = 0; i < bindings.length; i += 2)
                if (bindings[i] == null || bindings[i + 1] == null || bindings[i + 1].isEmpty() ||
                    names.putIfAbsent(bindings[i], bindings[i + 1]) != null) throw new IllegalArgumentException("World binding name");
        }
        this.loader = loader; clientType = load("class_310"); playerType = load("class_746");
        instance = mappedField(clientType, "field_1700", clientType, true);
        player = mappedField(clientType, "field_1724", playerType, false);
        world = mappedField(clientType, "field_1687", load("class_638"), false);
        screen = mappedField(clientType, "field_1755", load("class_437"), false);
        options = mappedField(clientType, "field_1690", load("class_315"), false);
        manager = mappedField(clientType, "field_1761", load("class_636"), false);
        onThread = mappedMethod(clientType, "method_18854", boolean.class);
        alive = mappedMethod(playerType, "method_5805", boolean.class);
    }
    private String name(String source) {
        if (names.isEmpty()) return source; // Existing 1.21.11 test/embedding constructor.
        String result = names.get(source);
        if (result == null) throw new IllegalArgumentException("Missing exact world binding: " + source);
        return result;
    }
    private Class<?> load(String source) throws ClassNotFoundException {
        return Class.forName(name("net/minecraft/" + source).replace('/', '.'), false, loader);
    }
    private Field mappedField(Class<?> owner, String source, Class<?> type, boolean stat) throws ReflectiveOperationException {
        return field(owner, name(source), type, stat);
    }
    private Method mappedMethod(Class<?> owner, String source, Class<?> result, Class<?>... args) throws ReflectiveOperationException {
        String mapped = name(source);
        try {
            Method m = owner.getMethod(mapped, args); // Includes public interface defaults such as raycast.
            if (m.getReturnType() != result || Modifier.isStatic(m.getModifiers()) || !m.trySetAccessible()) throw new IllegalAccessException(mapped);
            return m;
        } catch (NoSuchMethodException missing) { return method(owner, mapped, result, args); }
    }
    private static Field field(Class<?> owner, String name, Class<?> type, boolean stat) throws ReflectiveOperationException {
        for (Class<?> c = owner; c != null; c = c.getSuperclass()) try {
            Field f = c.getDeclaredField(name);
            if (f.getType() != type || Modifier.isStatic(f.getModifiers()) != stat || !f.trySetAccessible()) throw new IllegalAccessException(name);
            return f;
        } catch (NoSuchFieldException missing) { /* Exact inherited mapping only. */ }
        throw new NoSuchFieldException(name);
    }
    private static Method method(Class<?> owner, String name, Class<?> result, Class<?>... args) throws ReflectiveOperationException {
        for (Class<?> c = owner; c != null; c = c.getSuperclass()) try {
            Method m = c.getDeclaredMethod(name, args);
            if (m.getReturnType() != result || Modifier.isStatic(m.getModifiers()) || !m.trySetAccessible()) throw new IllegalAccessException(name);
            return m;
        } catch (NoSuchMethodException missing) { /* Exact inherited mapping only. */ }
        throw new NoSuchMethodException(name);
    }
    public void prepare(int feature) throws ReflectiveOperationException {
        if (stopped || (feature != 2 && feature != 4 && feature != 5 && feature != 7) || (feature == 2 && target != 12111)) throw new IllegalStateException("World feature/lifecycle");
        if (prepared[feature]) return;
        if (feature == 7) { prepareTotem(); prepared[7] = true; return; }
        if (feature == 5) { prepareJump(); prepared[5] = true; return; }
        if (feature == 4) { prepareTrigger(); prepared[4] = true; return; }
        Class<?> stack = load("class_1799");
        empty = method(stack, "method_7960", boolean.class); item = method(stack, "method_7909", load("class_1792"));
        count = method(stack, "method_7947", int.class); copy = method(stack, "method_7972", stack);
        same = stack.getDeclaredMethod("method_31577", stack, stack);
        if (same.getReturnType() != boolean.class || !Modifier.isStatic(same.getModifiers()) || !same.trySetAccessible()) throw new IllegalAccessException("stack comparison");
        {
            Class<?> inv = load("class_1661"); inventory = method(playerType, "method_31548", inv);
            selected = field(inv, "field_7545", int.class, false); main = field(inv, "field_7547", load("class_2371"), false);
            pearlItem = field(load("class_1802"), "field_8634", load("class_1792"), true);
            sync = method(load("class_636"), "method_2911", void.class);
            cooldown = field(clientType, "field_1752", int.class, false); use = method(clientType, "method_1583", void.class);
            usingItem = method(playerType, "method_6115", boolean.class);
        }
        prepared[feature] = true;
    }
    private Object client() throws ReflectiveOperationException {
        if (stopped) return null;
        Object c = instance.get(null); return c != null && (boolean) onThread.invoke(c) ? c : null;
    }
    private boolean live(Object c, Object p, Object w) throws ReflectiveOperationException {
        return p != null && w != null && screen.get(c) == null && (boolean) alive.invoke(p);
    }
    private long next() { if (++sequence >= (1L << 53)) { stop(); return 0; } return sequence; }
    private boolean equal(Object a, Object b) throws ReflectiveOperationException {
        return a != null && b != null && (int) count.invoke(a) == (int) count.invoke(b) && (boolean) same.invoke(null, a, b);
    }
    /** {owner,p,w,inventory,manager,stackCopies[9],double[13]:schema,nonce,selected,owned,pearlFlags[9]}. */
    public Object[] capturePearl() throws ReflectiveOperationException {
        Object c = client(); if (c == null) return null;
        pearlNonce = 0; prepare(2);
        Object p = player.get(c), w = world.get(c), m = manager.get(c);
        if (!live(c, p, w) || m == null || (boolean) usingItem.invoke(p)) return null;
        Object inv = inventory.invoke(p);
        if (inv == null || !(main.get(inv) instanceof List<?> stacks) || stacks.size() < 9) return null;
        int slot = selected.getInt(inv); if (slot < 0 || slot > 8) return null;
        boolean owned = pearlOwner != null && pearlOwner[0] == p && pearlOwner[1] == w && pearlOwner[2] == inv && pearlOwner[3] == m && (int) pearlOwner[5] == slot;
        if (pearlOwner != null && !owned) return null;
        Object[] copies = new Object[9]; double[] data = new double[13];
        data[0] = 1; data[1] = pearlNonce = next(); data[2] = slot; data[3] = owned ? 1 : 0;
        for (int i = 0; i < 9; ++i) {
            Object stack = stacks.get(i); if (stack == null) return null;
            copies[i] = copy.invoke(stack);
            data[4 + i] = !(boolean) empty.invoke(stack) && item.invoke(stack) == pearlItem.get(null) ? 1 : 0;
        }
        return stopped ? null : new Object[]{this, p, w, inv, m, copies, data};
    }
    public boolean beginPearl(Object[] packet, int slot) throws ReflectiveOperationException {
        if (client() == null || pearlOwner != null || packet == null || packet.length != 7 || packet[0] != this ||
            !(packet[6] instanceof double[] data) || data.length != 13 || data[0] != 1 || data[1] <= 0 || data[1] != pearlNonce || slot < 0 || slot > 8) return false;
        pearlNonce = 0; Object[] now = capturePearl(); pearlNonce = 0;
        if (now == null || ((double[]) now[6])[2] != data[2] || ((double[]) now[6])[4 + slot] != 1) return false;
        for (int i = 1; i <= 4; ++i) if (packet[i] != now[i]) return false;
        Object stack = ((Object[]) now[5])[slot];
        if (!equal(((Object[]) packet[5])[slot], stack)) return false;
        pearlOwner = new Object[]{now[1], now[2], now[3], now[4], (int) data[2], slot, stack, false};
        if ((int) data[2] != slot) { selected.setInt(now[3], slot); sync.invoke(now[4]); }
        return true;
    }
    public boolean usePearl() throws ReflectiveOperationException {
        if (pearlOwner == null || (boolean) pearlOwner[7]) return false;
        Object[] now = capturePearl(); pearlNonce = 0;
        if (now == null || ((double[]) now[6])[3] != 1) return false;
        int slot = (int) pearlOwner[5];
        if (((double[]) now[6])[4 + slot] != 1 || !equal(((Object[]) now[5])[slot], pearlOwner[6])) return false;
        pearlOwner[7] = true; // Non-repeatable even if a mod throws after the use was consumed.
        Object c = client(); cooldown.setInt(c, 0); use.invoke(c); return true;
    }
    public boolean releasePearl() throws ReflectiveOperationException {
        pearlNonce = 0;
        if (pearlOwner == null) return true;
        Object c = client(); if (c == null) return false;
        Object[] owner = pearlOwner;
        if (player.get(c) == owner[0] && world.get(c) == owner[1] && manager.get(c) == owner[3] && inventory.invoke(owner[0]) == owner[2] &&
            selected.getInt(owner[2]) == (int) owner[5] && (int) owner[4] != (int) owner[5]) {
            // Consume before invoking an overridable synchronizer; no delayed restore replay.
            pearlOwner = null; selected.setInt(owner[2], (int) owner[4]); sync.invoke(owner[3]);
        } else pearlOwner = null;
        return true;
    }
    private static Constructor<?> constructor(Class<?> owner, Class<?>... args) throws ReflectiveOperationException {
        Constructor<?> c = owner.getDeclaredConstructor(args);
        if (!c.trySetAccessible()) throw new IllegalAccessException("World constructor");
        return c;
    }
    // Triggerbot owns its complete decision loop in Java. The native callback
    // only exposes shared friend/bot services, attack ownership and other
    // modules' existing weapon preparation; it never chooses a trigger attack.
    private native long triggerService(int operation, Object p, Object w, Object e, int value, long epoch);
    private Class<?> triggerLiving, triggerPlayerType, triggerMob, triggerAnimal, triggerAxe, triggerHitType;
    private Field triggerCrosshair, triggerHitEntity, triggerSelected, triggerFall, triggerTicks, triggerYaw, triggerPos;
    private Field triggerX, triggerZ;
    private Method triggerAlive, triggerInvisible, triggerUsing, triggerInventory, triggerStack, triggerEmpty, triggerItem;
    private Method triggerCooldown, triggerGround, triggerWater, triggerSwimming, triggerFlying, triggerClimbing;
    private Method triggerRiding, triggerSprinting, triggerBlind, triggerBlocking, triggerId, triggerCamera, triggerPick;
    private Method triggerBlockRange, triggerEntityRange, triggerAttack, triggerSwing, triggerEntry, triggerInTag;
    private Method triggerGetWindow, triggerGetHandle, triggerGlfwMouse;
    private Object triggerBlindness, triggerHand, triggerSwordTag;
    private Object triggerPlayer, triggerWorld, triggerManager, triggerTarget, triggerInv;
    private long triggerEpoch, triggerGeneration, triggerManual, triggerReady, triggerCritical = -1, triggerNotBefore;
    private int triggerSlot = -1, triggerLastTicks = -1, triggerPreparedSlot = -1, triggerPreparationTicks = -1;
    private float triggerThreshold, triggerLastCooldown;
    private boolean triggerFailed;
    private boolean triggerSimulationPrepared;
    private Field triggerMouse, triggerMouseLeft, triggerBindings, triggerBindingPressed, triggerBindingCount, triggerBindingKey;
    private Method triggerMouseCallback, triggerKeyCode, triggerKeyCategory;
    private Constructor<?> triggerMouseInputConstructor;
    private Object triggerMouseKeyType;
    private boolean triggerTickHooked;
    private boolean clientTickHooked;
    private volatile boolean triggerActive;
    private volatile long triggerLastConfigTime;
    private volatile long triggerGenerationArg, triggerManualArg;
    private volatile float triggerLowArg = .9f, triggerHighArg = 1.f;
    private volatile int triggerReactionLowArg, triggerReactionHighArg;
    private volatile int triggerTargetsArg, triggerWeaponArg, triggerCriticalsArg;
    private volatile boolean triggerOnClickArg, triggerSwingHandArg, triggerClickSimulationArg;

    private void prepareTrigger() throws ReflectiveOperationException {
        Class<?> entity = load("class_1297"), inv = load("class_1661"), stack = load("class_1799");
        Class<?> itemType = load("class_1792"), vec = load("class_243"), hand = load("class_1268");
        Class<?> hit = load("class_239"), m = load("class_636"), entry = load("class_6880");
        triggerLiving = load("class_1309"); triggerPlayerType = load("class_1657");
        triggerMob = load("class_1308"); triggerAnimal = load("class_1429"); triggerAxe = load("class_1743");
        triggerHitType = load("class_3966");
        triggerCrosshair = mappedField(clientType, "field_1765", hit, false);
        triggerHitEntity = mappedField(triggerHitType, "field_17592", entity, false);
        triggerSelected = mappedField(inv, "field_7545", int.class, false);
        triggerFall = mappedField(entity, "field_6017", double.class, false);
        triggerTicks = mappedField(triggerLiving, "field_6273", int.class, false);
        triggerYaw = mappedField(entity, "field_6031", float.class, false);
        triggerPos = mappedField(entity, "field_22467", vec, false);
        triggerX = mappedField(vec, "field_1352", double.class, false);
        triggerZ = mappedField(vec, "field_1350", double.class, false);
        triggerAlive = mappedMethod(entity, "method_5805", boolean.class);
        triggerInvisible = mappedMethod(entity, "method_5767", boolean.class);
        triggerUsing = mappedMethod(playerType, "method_6115", boolean.class);
        triggerInventory = mappedMethod(playerType, "method_31548", inv);
        triggerStack = mappedMethod(inv, "method_5438", stack, int.class);
        triggerEmpty = mappedMethod(stack, "method_7960", boolean.class);
        triggerItem = mappedMethod(stack, "method_7909", itemType);
        triggerCooldown = mappedMethod(playerType, "method_7261", float.class, float.class);
        triggerGround = mappedMethod(entity, "method_24828", boolean.class);
        triggerWater = mappedMethod(entity, "method_5799", boolean.class);
        triggerSwimming = mappedMethod(entity, "method_5681", boolean.class);
        triggerFlying = mappedMethod(triggerLiving, "method_6128", boolean.class);
        triggerClimbing = mappedMethod(triggerLiving, "method_6101", boolean.class);
        triggerRiding = mappedMethod(entity, "method_5765", boolean.class);
        triggerSprinting = mappedMethod(entity, "method_5624", boolean.class);
        triggerBlind = mappedMethod(triggerLiving, "method_6059", boolean.class, entry);
        triggerBlindness = mappedField(load("class_1294"), "field_5919", entry, true).get(null);
        triggerBlocking = mappedMethod(triggerLiving, "method_6039", boolean.class);
        triggerId = mappedMethod(entity, "method_5628", int.class);
        triggerCamera = mappedMethod(clientType, "method_1560", entity);
        triggerBlockRange = mappedMethod(playerType, "method_55754", double.class);
        triggerEntityRange = mappedMethod(playerType, "method_55755", double.class);
        triggerAttack = mappedMethod(m, "method_2918", void.class, triggerPlayerType, entity);
        triggerSwing = mappedMethod(playerType, "method_6104", void.class, hand);
        triggerHand = mappedField(hand, "field_5808", hand, true).get(null);
        Class<?> window = load("class_1041");
        triggerGetWindow = mappedMethod(clientType, "method_22683", window);
        triggerGetHandle = mappedMethod(window, "method_4490", long.class);
        Class<?> glfw = Class.forName("org.lwjgl.glfw.GLFW", false, loader);
        triggerGlfwMouse = glfw.getMethod("glfwGetMouseButton", long.class, int.class);
        if (!Modifier.isStatic(triggerGlfwMouse.getModifiers()) || triggerGlfwMouse.getReturnType() != int.class ||
            !triggerGlfwMouse.trySetAccessible()) throw new IllegalAccessException("GLFW mouse input");
        triggerPick = playerType.getDeclaredMethod(name("method_76763"), entity, double.class, double.class, float.class);
        if (!Modifier.isStatic(triggerPick.getModifiers()) || triggerPick.getReturnType() != hit ||
            !triggerPick.trySetAccessible()) throw new IllegalAccessException("Static crosshair query");
        Class<?> reference = load("class_6880$class_6883"), tag = load("class_6862");
        triggerEntry = mappedMethod(itemType, "method_40131", reference);
        triggerInTag = mappedMethod(reference, "method_40220", boolean.class, tag);
        triggerSwordTag = mappedField(load("class_3489"), "field_42611", tag, true).get(null);
        if (triggerHand == null || triggerBlindness == null || triggerSwordTag == null)
            throw new IllegalStateException("Uninitialized trigger bindings");
        prepareTriggerLifecycle();
    }

    private void prepareTickLifecycle() {
        if (clientTickHooked) return;
        try {
            Class<?> eventsClass = Class.forName("net.fabricmc.fabric.api.client.event.lifecycle.v1.ClientTickEvents", true, loader);
            Class<?> listenerType = Class.forName("net.fabricmc.fabric.api.client.event.lifecycle.v1.ClientTickEvents$StartTick", true, loader);
            Object event = eventsClass.getField("START_CLIENT_TICK").get(null);
            ClassLoader proxyLoader = listenerType.getClassLoader() != null ? listenerType.getClassLoader() : WorldBridge.class.getClassLoader();
            Object proxy = Proxy.newProxyInstance(proxyLoader, new Class<?>[]{listenerType}, (p, m, a) -> {
                if (m.getDeclaringClass() == Object.class) {
                    if (m.getName().equals("hashCode")) return System.identityHashCode(p);
                    if (m.getName().equals("equals")) return p == a[0];
                    return "WorldBridgeTick";
                }
                if (a != null && a.length >= 1) {
                    onTriggerTick(a[0]);
                }
                return null;
            });
            Class.forName("net.fabricmc.fabric.api.event.Event", true, loader).getMethod("register", Object.class).invoke(event, proxy);
            clientTickHooked = true;
            triggerTickHooked = true;
        } catch (Throwable ignored) {
            clientTickHooked = false;
            triggerTickHooked = false;
        }
    }

    private void prepareTriggerLifecycle() {
        prepareTickLifecycle();
    }

    private void onTriggerTick(Object c) {
        if (!triggerActive || triggerFailed || stopped || c == null) return;
        final long now = System.nanoTime();
        if (now - triggerLastConfigTime > 200_000_000L) {
            try { cancelTrigger(); } catch (ReflectiveOperationException ignored) {}
            return;
        }
        try {
            executeTrigger(c, triggerGenerationArg, triggerManualArg,
                triggerLowArg, triggerHighArg, triggerReactionLowArg, triggerReactionHighArg,
                triggerTargetsArg, triggerWeaponArg, triggerCriticalsArg,
                triggerOnClickArg, triggerSwingHandArg, triggerClickSimulationArg);
        } catch (Throwable t) {
            triggerFailed = true;
            try { cancelTrigger(); } catch (Throwable cleanup) { t.addSuppressed(cleanup); }
        }
    }

    private void prepareTriggerSimulation() throws ReflectiveOperationException {
        if (triggerSimulationPrepared) return;
        Class<?> mouse = load("class_312"), binding = load("class_304");
        Class<?> key = load("class_3675$class_306"), type = load("class_3675$class_307");
        triggerMouse = mappedField(clientType, "field_1729", mouse, false);
        triggerMouseLeft = mappedField(mouse, "field_1791", boolean.class, false);
        triggerBindings = mappedField(binding, "field_1657", Map.class, true);
        triggerBindingPressed = mappedField(binding, "field_1653", boolean.class, false);
        triggerBindingCount = mappedField(binding, "field_1661", int.class, false);
        triggerBindingKey = mappedField(binding, "field_1655", key, false);
        triggerKeyCode = mappedMethod(key, "method_1444", int.class);
        triggerKeyCategory = mappedMethod(key, "method_1442", type);
        triggerMouseKeyType = mappedField(type, "field_1672", type, true).get(null);
        if (triggerMouseKeyType == null) throw new IllegalStateException("Mouse input category");
        {
            Class<?> input = load("class_11910");
            triggerMouseInputConstructor = constructor(input, int.class, int.class);
            triggerMouseCallback = mappedMethod(mouse, "method_1601", void.class, long.class, input, int.class);
        }
        triggerSimulationPrepared = true;
    }
    private void triggerMouseEdge(Object mouse, long handle, int action, Object input) throws ReflectiveOperationException {
        triggerMouseCallback.invoke(mouse, handle, input, action);
    }
    private void simulateTriggerClick(Object c, Object p, Object w, long generation, long manual) throws ReflectiveOperationException {
        // Only notify the game's mouse callback after the single direct attack.
        // A synchronous pulse has no delayed UP and no worker outliving disable.
        if (client() != c || !live(c, p, w) || player.get(c) != p || world.get(c) != w ||
            triggerService(8, p, w, null, 0, generation) != manual) return;
        Object mouse = triggerMouse.get(c), window = triggerGetWindow.invoke(c);
        long handle = window == null ? 0 : (long) triggerGetHandle.invoke(window);
        Object registry = triggerBindings.get(null);
        if (mouse == null || handle == 0 || !(registry instanceof Map<?, ?> mappings)) return;
        java.util.ArrayList<Object[]> snapshots = new java.util.ArrayList<>();
        for (Object binding : mappings.values()) {
            if (binding == null) continue;
            Object key = triggerBindingKey.get(binding);
            if (key != null && triggerKeyCategory.invoke(key) == triggerMouseKeyType && (int) triggerKeyCode.invoke(key) == 0)
                snapshots.add(new Object[]{binding, key, triggerBindingPressed.getBoolean(binding), triggerBindingCount.getInt(binding)});
        }
        Object input = triggerMouseInputConstructor.newInstance(0, 0);
        boolean held = triggerMouseLeft.getBoolean(mouse);
        ReflectiveOperationException error = null;
        try {
            triggerMouseEdge(mouse, handle, 1, input);
        } catch (ReflectiveOperationException failure) { error = failure; }
        finally {
            // Always finish the pulse, even if a callback threw after its DOWN.
            try { triggerMouseEdge(mouse, handle, 0, input); }
            catch (ReflectiveOperationException failure) { if (error == null) error = failure; else error.addSuppressed(failure); }
            boolean sameSession = false;
            try {
                sameSession = client() == c && player.get(c) == p && world.get(c) == w && screen.get(c) == null &&
                    triggerGetWindow.invoke(c) == window && (long) triggerGetHandle.invoke(window) == handle &&
                    triggerService(8, p, w, null, 0, generation) == manual;
            } catch (ReflectiveOperationException failure) { if (error == null) error = failure; else error.addSuppressed(failure); }
            // Restore every left-mouse binding, including a rebound use key or
            // modded binding. Preserve pre-existing queued manual presses; do
            // not leave another vanilla attack queued after attackEntity.
            for (Object[] state : snapshots) try {
                if (triggerBindingKey.get(state[0]) == state[1]) {
                    triggerBindingPressed.setBoolean(state[0], sameSession && (boolean) state[2]);
                    triggerBindingCount.setInt(state[0], sameSession ? (int) state[3] : 0);
                }
            } catch (ReflectiveOperationException failure) { if (error == null) error = failure; else error.addSuppressed(failure); }
            try { triggerMouseLeft.setBoolean(mouse, sameSession && held); }
            catch (ReflectiveOperationException failure) { if (error == null) error = failure; else error.addSuppressed(failure); }
        }
        if (error != null) throw error;
    }
    private boolean triggerLeftDown(Object c) throws ReflectiveOperationException {
        Object window = triggerGetWindow.invoke(c);
        if (window == null) return false;
        long handle = (long) triggerGetHandle.invoke(window);
        // GLFW_MOUSE_BUTTON_LEFT = 0; GLFW_PRESS = 1. Poll only on the client thread.
        return handle != 0 && (int) triggerGlfwMouse.invoke(null, handle, 0) == 1;
    }
    private boolean triggerSword(Object s) throws ReflectiveOperationException {
        if (s == null || (boolean) triggerEmpty.invoke(s)) return false;
        Object i = triggerItem.invoke(s);
        if (i == null) return false;
        Object entry = triggerEntry.invoke(i);
        return entry != null && (boolean) triggerInTag.invoke(entry, triggerSwordTag);
    }
    private boolean triggerAxe(Object s) throws ReflectiveOperationException {
        return s != null && !(boolean) triggerEmpty.invoke(s) && triggerAxe.isInstance(triggerItem.invoke(s));
    }
    private boolean triggerWeapon(Object p, Object w, Object inv, int slot, int weapon) throws ReflectiveOperationException {
        int original = (int) triggerService(6, p, w, null, slot, 0);
        if (original >= 0 && original < 9) slot = original;
        Object s = triggerStack.invoke(inv, slot);
        return weapon == 0 || (weapon == 1 ? triggerSword(s) : triggerAxe(s));
    }
    private boolean triggerShield(Object p, Object e) throws ReflectiveOperationException {
        if (!triggerPlayerType.isInstance(e) || !(boolean) triggerBlocking.invoke(e)) return false;
        Object a = triggerPos.get(p), b = triggerPos.get(e);
        if (a == null || b == null) throw new IllegalStateException("Trigger shield position");
        double dx = triggerX.getDouble(a) - triggerX.getDouble(b), dz = triggerZ.getDouble(a) - triggerZ.getDouble(b);
        float yaw = triggerYaw.getFloat(e);
        if (!Double.isFinite(dx) || !Double.isFinite(dz) || !Float.isFinite(yaw)) throw new IllegalStateException("Trigger shield yaw");
        return Math.abs(Math.IEEEremainder(yaw - Math.toDegrees(Math.atan2(-dx, dz)), 360.)) < 90.;
    }
    private Object triggerFresh(Object c, Object p) throws ReflectiveOperationException {
        if (triggerCamera.invoke(c) != p) return null;
        double blocks = (double) triggerBlockRange.invoke(p), entities = (double) triggerEntityRange.invoke(p);
        if (!Double.isFinite(blocks) || !Double.isFinite(entities) || blocks <= 0 || entities <= 0) return null;
        Object hit = triggerPick.invoke(null, p, blocks, entities, 1.f);
        return triggerHitType.isInstance(hit) ? triggerHitEntity.get(hit) : null;
    }
    private boolean triggerEligible(Object c, Object p, Object w, Object m, Object e, int targets) throws ReflectiveOperationException {
        if (c == null || !live(c, p, w) || m == null || e == null || e == p ||
            !triggerLiving.isInstance(e) || !(boolean) triggerAlive.invoke(e) || (boolean) triggerInvisible.invoke(e) ||
            (boolean) triggerUsing.invoke(p)) return false;
        boolean isPlayer = triggerPlayerType.isInstance(e), animal = triggerAnimal.isInstance(e), mob = triggerMob.isInstance(e);
        if (!(isPlayer || (targets >= 1 && mob && !animal) || (targets == 2 && animal))) return false;
        if (isPlayer && triggerService(0, p, w, e, 0, 0) == 0) return false;
        return triggerFresh(c, p) == e;
    }
    private void triggerCycle(long now, float low, float high, int reactionLow, int reactionHigh) {
        java.util.concurrent.ThreadLocalRandom r = java.util.concurrent.ThreadLocalRandom.current();
        triggerThreshold = low == high ? low : (float) r.nextDouble(low, high);
        triggerReady = now + (reactionLow == reactionHigh ? reactionLow : r.nextInt(reactionLow, reactionHigh + 1)) * 1_000_000L;
        triggerCritical = -1;
    }
    public void cancelTrigger() throws ReflectiveOperationException {
        triggerActive = false;
        Object c = client(); if (c == null) return;
        // Do not retain targets or JNI objects across menus, focus/session loss or disable.
        triggerService(5, triggerPlayer != null ? triggerPlayer : player.get(c),
            triggerWorld != null ? triggerWorld : world.get(c), null, 0, 0);
        triggerPlayer = triggerWorld = triggerManager = triggerTarget = triggerInv = null;
        triggerSlot = triggerPreparedSlot = triggerPreparationTicks = triggerLastTicks = -1; triggerLastCooldown = 0; triggerCritical = -1;
        triggerReady = 0;
    }
    public void tickTrigger(boolean active, long generation, long manual,
        float low, float high, int reactionLow, int reactionHigh, int targets, int weapon, int criticals, boolean onClick, boolean swingHand, boolean clickSimulation)
        throws ReflectiveOperationException {
        Object c = client(); if (c == null) return;
        if (!active) { cancelTrigger(); return; }
        if (triggerFailed) return;
        if (!Float.isFinite(low) || !Float.isFinite(high) || low < .5f || high > 1.f || low > high ||
            reactionLow < 0 || reactionHigh > 150 || reactionLow > reactionHigh || targets < 0 || targets > 2 ||
            weapon < 0 || weapon > 2 || criticals < 0 || criticals > 2) { cancelTrigger(); return; }
        prepare(4);
        triggerActive = true;
        triggerLastConfigTime = System.nanoTime();
        triggerGenerationArg = generation;
        triggerManualArg = manual;
        triggerLowArg = low;
        triggerHighArg = high;
        triggerReactionLowArg = reactionLow;
        triggerReactionHighArg = reactionHigh;
        triggerTargetsArg = targets;
        triggerWeaponArg = weapon;
        triggerCriticalsArg = criticals;
        triggerOnClickArg = onClick;
        triggerSwingHandArg = swingHand;
        triggerClickSimulationArg = clickSimulation;
        if (!triggerTickHooked) {
            executeTrigger(c, generation, manual, low, high, reactionLow, reactionHigh,
                targets, weapon, criticals, onClick, swingHand, clickSimulation);
        }
    }

    private void executeTrigger(Object c, long generation, long manual,
        float low, float high, int reactionLow, int reactionHigh, int targets, int weapon, int criticals,
        boolean onClick, boolean swingHand, boolean clickSimulation) throws ReflectiveOperationException {
        final long now = System.nanoTime();
        try {
            if (onClick && !triggerLeftDown(c)) { cancelTrigger(); return; }
            if (clickSimulation) prepareTriggerSimulation();
            Object p = player.get(c), w = world.get(c), m = manager.get(c), hit = triggerCrosshair.get(c);
            Object e = triggerHitType.isInstance(hit) ? triggerHitEntity.get(hit) : null;
            if (!triggerEligible(c, p, w, m, e, targets)) { cancelTrigger(); return; }
            Object inv = triggerInventory.invoke(p);
            int slot = inv == null ? -1 : triggerSelected.getInt(inv);
            if (slot < 0 || slot > 8 || !triggerWeapon(p, w, inv, slot, weapon)) { cancelTrigger(); return; }
            float cd = (float) triggerCooldown.invoke(p, 0.f);
            int ticks = triggerTicks.getInt(p);
            if (!Float.isFinite(cd) || cd < 0 || cd > 1 || ticks < 0) { cancelTrigger(); return; }
            if (triggerPlayer != p || triggerWorld != w || triggerManager != m || triggerTarget != e ||
                triggerInv != inv || triggerSlot != slot || triggerGeneration != generation) {
                cancelTrigger();
                triggerPlayer = p; triggerWorld = w; triggerManager = m; triggerTarget = e; triggerInv = inv;
                triggerSlot = slot; triggerGeneration = generation; triggerManual = manual;
                triggerLastTicks = ticks; triggerLastCooldown = cd;
                if (++triggerEpoch <= 0) throw new IllegalStateException("Trigger epoch exhausted");
                triggerCycle(now, low, high, reactionLow, reactionHigh);
            }
            int id = (int) triggerId.invoke(e);
            if (id < 0 || triggerService(1, p, w, e, id, triggerEpoch) == 0) { cancelTrigger(); return; }
            boolean consumed = triggerPreparedSlot < 0 && (ticks < triggerLastTicks || (triggerLastCooldown >= .5f && cd < .25f));
            triggerLastTicks = ticks; triggerLastCooldown = cd;
            if (manual != triggerManual || consumed) {
                triggerManual = manual;
                if (triggerPreparedSlot >= 0) { cancelTrigger(); return; }
                triggerCycle(now, low, high, reactionLow, reactionHigh); return;
            }
            if (triggerPreparedSlot >= 0 && ticks == triggerPreparationTicks) return;
            if ((triggerNotBefore != 0 && now - triggerNotBefore < 0) || now - triggerReady < 0 || cd < triggerThreshold) return;
            boolean ground = (boolean) triggerGround.invoke(p);
            double fall = ((Number) triggerFall.get(p)).doubleValue();
            if (!Double.isFinite(fall)) { cancelTrigger(); return; }
            boolean possible = !(boolean) triggerWater.invoke(p) && !(boolean) triggerSwimming.invoke(p) &&
                !(boolean) triggerFlying.invoke(p) && !(boolean) triggerClimbing.invoke(p) &&
                !(boolean) triggerRiding.invoke(p) && !(boolean) triggerSprinting.invoke(p) &&
                !(boolean) triggerBlind.invoke(p, triggerBlindness);
            boolean critical = possible && !ground && fall > 0;
            if (criticals == 2 && !critical) return;
            if (criticals == 1 && !ground && possible && !critical) {
                if (triggerCritical < 0) triggerCritical = now;
                if (now - triggerCritical < 150_000_000L) return;
            } else triggerCritical = -1;
            boolean shield = triggerShield(p, e);
            if (triggerPreparedSlot >= 0) {
                if (triggerService(7, p, w, e, triggerPreparedSlot, 0) <= 0) { cancelTrigger(); return; }
            } else {
                int chosen = shield && !triggerAxe(triggerStack.invoke(inv, slot))
                    ? (int) triggerService(2, p, w, e, 0, 0) : -1;
                if (chosen >= 0 && chosen < 9) {
                    triggerPreparedSlot = triggerSlot = chosen;
                    triggerPreparationTicks = ticks;
                    triggerNotBefore = now + 25_000_000L; return;
                }
                if (shield && !triggerAxe(triggerStack.invoke(inv, slot))) return;
            }
            // All checks and the vanilla calls happen synchronously on the client
            // thread. Never queue a Windows click, replay failures or catch up.
            Object current = client();
            float finalCooldown = (float) triggerCooldown.invoke(p, 0.f);
            if (current != c || player.get(c) != p || world.get(c) != w || manager.get(c) != m ||
                triggerInventory.invoke(p) != inv || triggerSelected.getInt(inv) != triggerSlot ||
                !triggerEligible(c, p, w, m, e, targets) ||
                !Float.isFinite(finalCooldown) || finalCooldown > 1.f || finalCooldown < triggerThreshold ||
                triggerService(8, p, w, e, 0, generation) != manual ||
                (onClick && !triggerLeftDown(c)) ||
                (triggerShield(p, e) && !triggerAxe(triggerStack.invoke(inv, triggerSlot)))) { cancelTrigger(); return; }
            // Reserve the next cycle before a mutating call: an exception may
            // follow an already sent packet, so this feature then suspends.
            triggerNotBefore = now + 50_000_000L;
            triggerAttack.invoke(m, p, e);
            triggerSwing.invoke(p, triggerHand);
            if (clickSimulation) simulateTriggerClick(c, p, w, generation, manual);
            triggerService(4, p, w, e, 0, 0);
            triggerPreparedSlot = triggerPreparationTicks = -1;
            triggerLastTicks = triggerTicks.getInt(p);
            triggerLastCooldown = (float) triggerCooldown.invoke(p, 0.f);
            triggerCycle(now, low, high, reactionLow, reactionHigh);
        } catch (ReflectiveOperationException | RuntimeException error) {
            triggerFailed = true;
            try { cancelTrigger(); } catch (ReflectiveOperationException cleanup) { error.addSuppressed(cleanup); }
            throw error;
        }
    }

    // Independent OneStep controller; Argon AutoJumpReset is a behavior reference.
    // Physical input is read from GLFW; all game input writes stay in Java.
    // The native callback exposes authorization/cancellation only.
    protected native boolean jumpPermitted(long generation);
    private Class<?> jumpLiving;
    private Method jumpGetWindow, jumpGetHandle, jumpGlfwKey, jumpGlfwMouse, jumpGlfwScan, jumpGlfwAttrib;
    private int jumpScanCode = -1, jumpScanSymbol = -1;
    private long jumpWindowHandle;
    private Field jumpKey, jumpBound, jumpSymbols, jumpScans, jumpMouse, jumpHurt, jumpAge, jumpCooldown;
    private Field jumpAllowFlight, jumpFlying;
    private Method jumpCode, jumpCategory, jumpPressed, jumpSetPressed, jumpGround, jumpUsing, jumpSleeping;
    private Method jumpWater, jumpSwimming, jumpFallFlying, jumpClimbing, jumpRiding, jumpAbilities, jumpSource, jumpAttacker;
    private Object jumpPlayer, jumpWorld, jumpOptions, jumpBinding, jumpInput, jumpPreviousSource;
    private Object[] jumpOwner;
    private long jumpGeneration, jumpSourceRevision, jumpConsumedSource, jumpSourceAt;
    private long jumpReadyAt, jumpExpiresAt, jumpConfirmUntil, jumpHoldUntil, jumpNextAllowed;
    private long jumpLastGrounded = -1, jumpDamageAt = -1;
    private int jumpPreviousHurt, jumpSentAge, jumpMode, jumpChance, jumpDelay;
    private boolean jumpSeeded, jumpPending, jumpConfirmed, jumpHolding, jumpSourceLiving;
    private boolean jumpAwaitingSource, jumpOnlyCombat, jumpFailed;

    private void prepareJump() throws ReflectiveOperationException {
        Class<?> entity = load("class_1297"), binding = load("class_304");
        Class<?> input = load("class_3675$class_306"), type = load("class_3675$class_307");
        Class<?> abilities = load("class_1656"), source = load("class_1282");
        jumpLiving = load("class_1309");
        jumpKey = mappedField(load("class_315"), "field_1903", binding, false);
        jumpBound = mappedField(binding, "field_1655", input, false);
        jumpSymbols = mappedField(type, "field_1668", type, true);
        jumpScans = mappedField(type, "field_1671", type, true);
        jumpMouse = mappedField(type, "field_1672", type, true);
        jumpCode = mappedMethod(input, "method_1444", int.class);
        jumpCategory = mappedMethod(input, "method_1442", type);
        jumpPressed = mappedMethod(binding, "method_1434", boolean.class);
        jumpSetPressed = mappedMethod(binding, "method_23481", void.class, boolean.class);
        jumpHurt = mappedField(jumpLiving, "field_6235", int.class, false);
        jumpAge = mappedField(entity, "field_6012", int.class, false);
        jumpCooldown = mappedField(jumpLiving, "field_6228", int.class, false);
        jumpGround = mappedMethod(entity, "method_24828", boolean.class);
        jumpUsing = mappedMethod(jumpLiving, "method_6115", boolean.class);
        jumpSleeping = mappedMethod(jumpLiving, "method_6113", boolean.class);
        jumpWater = mappedMethod(entity, "method_5799", boolean.class);
        jumpSwimming = mappedMethod(entity, "method_5681", boolean.class);
        jumpFallFlying = mappedMethod(jumpLiving, "method_6128", boolean.class);
        jumpClimbing = mappedMethod(jumpLiving, "method_6101", boolean.class);
        jumpRiding = mappedMethod(entity, "method_5765", boolean.class);
        jumpAbilities = mappedMethod(playerType, "method_31549", abilities);
        jumpAllowFlight = mappedField(abilities, "field_7478", boolean.class, false);
        jumpFlying = mappedField(abilities, "field_7479", boolean.class, false);
        jumpSource = mappedMethod(jumpLiving, "method_6081", source);
        jumpAttacker = mappedMethod(source, "method_5529", entity);
        Class<?> window = load("class_1041");
        jumpGetWindow = mappedMethod(clientType, "method_22683", window);
        jumpGetHandle = mappedMethod(window, "method_4490", long.class);
        Class<?> glfw = Class.forName("org.lwjgl.glfw.GLFW", false, loader);
        jumpGlfwKey = jumpGlfwMethod(glfw, "glfwGetKey", long.class, int.class);
        jumpGlfwMouse = jumpGlfwMethod(glfw, "glfwGetMouseButton", long.class, int.class);
        jumpGlfwScan = jumpGlfwMethod(glfw, "glfwGetKeyScancode", int.class);
        jumpGlfwAttrib = jumpGlfwMethod(glfw, "glfwGetWindowAttrib", long.class, int.class);
    }
    private static Method jumpGlfwMethod(Class<?> glfw, String name, Class<?>... args) throws ReflectiveOperationException {
        Method m = glfw.getMethod(name, args);
        if (!Modifier.isStatic(m.getModifiers()) || m.getReturnType() != int.class || !m.trySetAccessible())
            throw new IllegalAccessException(name);
        return m;
    }
    private long jumpWindow(Object c) throws ReflectiveOperationException {
        Object window = jumpGetWindow.invoke(c);
        long handle = window == null ? 0 : (long) jumpGetHandle.invoke(window);
        // GLFW_FOCUSED. Never restore a held key into an unfocused/replaced window.
        return handle != 0 && (int) jumpGlfwAttrib.invoke(null, handle, 0x00020001) == 1 ? handle : 0;
    }
    private static boolean jumpKnownSymbol(int key) {
        return key == 32 || key == 39 || (key >= 44 && key <= 57) || key == 59 || key == 61 ||
            (key >= 65 && key <= 93) || key == 96 || key == 161 || key == 162 ||
            (key >= 256 && key <= 269) || (key >= 280 && key <= 284) || (key >= 290 && key <= 314) ||
            (key >= 320 && key <= 336) || (key >= 340 && key <= 348);
    }
    private int[] jumpDescriptor(Object input) throws ReflectiveOperationException {
        if (input == null) return null;
        Object type = jumpCategory.invoke(input); int value = (int) jumpCode.invoke(input);
        if (type == jumpSymbols.get(null) && jumpKnownSymbol(value)) return new int[]{0, value};
        if (type == jumpScans.get(null) && value > 0 && value < 512) return new int[]{1, value};
        if (type == jumpMouse.get(null) && value >= 0 && value < 8) return new int[]{2, value};
        return null;
    }
    private boolean jumpPhysical(Object input, long handle) throws ReflectiveOperationException {
        int[] descriptor = jumpDescriptor(input);
        if (descriptor == null || handle == 0) return true;
        int kind = descriptor[0], key = descriptor[1];
        if (kind == 1) {
            if (jumpScanCode != key) {
                jumpScanCode = key; jumpScanSymbol = -1;
                for (int symbol = 32; symbol <= 348; ++symbol)
                    if (jumpKnownSymbol(symbol) && (int) jumpGlfwScan.invoke(null, symbol) == key) {
                        jumpScanSymbol = symbol; break;
                    }
            }
            if (jumpScanSymbol < 0) return true;
            key = jumpScanSymbol;
        }
        int state = (int) (kind == 2 ? jumpGlfwMouse : jumpGlfwKey).invoke(null, handle, key);
        if (state != 0 && state != 1) throw new IllegalStateException("Invalid GLFW input state");
        return state == 1;
    }
    private boolean jumpSpace(Object input) throws ReflectiveOperationException {
        int[] descriptor = jumpDescriptor(input);
        return descriptor != null && ((descriptor[0] == 0 && descriptor[1] == 32) ||
            (descriptor[0] == 1 && descriptor[1] == (int) jumpGlfwScan.invoke(null, 32)));
    }
    private boolean jumpPlayable(Object c, Object p, Object w) throws ReflectiveOperationException {
        if (!live(c, p, w) || (boolean) jumpUsing.invoke(p) || (boolean) jumpSleeping.invoke(p) ||
            (boolean) jumpWater.invoke(p) || (boolean) jumpSwimming.invoke(p) ||
            (boolean) jumpFallFlying.invoke(p) || (boolean) jumpClimbing.invoke(p) || (boolean) jumpRiding.invoke(p)) return false;
        Object abilities = jumpAbilities.invoke(p);
        return abilities != null && !jumpAllowFlight.getBoolean(abilities) && !jumpFlying.getBoolean(abilities);
    }
    private boolean releaseJump() throws ReflectiveOperationException {
        if (jumpOwner != null) {
            Object c = client(); if (c == null) return false;
            // Clear our old synthetic press even if the binding object was replaced.
            // If that same binding was rebound, preserve the NEW physical key state.
            boolean current=options.get(c) == jumpOwner[0] && jumpKey.get(jumpOwner[0]) == jumpOwner[1];
            Object input=current ? jumpBound.get(jumpOwner[1]) : null;
            boolean physical=current && jumpDescriptor(input)!=null && jumpWindow(c)==(long)jumpOwner[3] &&
                jumpPhysical(input,(long)jumpOwner[3]);
            jumpSetPressed.invoke(jumpOwner[1],physical);
            jumpOwner = null;
        }
        return true;
    }
    public void cancelJump() throws ReflectiveOperationException {
        if (!prepared[5] || client() == null) return;
        try { releaseJump(); }
        finally {
            jumpSeeded = jumpPending = jumpConfirmed = jumpHolding = false;
            jumpPlayer = jumpWorld = jumpOptions = jumpBinding = jumpInput = jumpPreviousSource = null;
            jumpSourceRevision = jumpConsumedSource = 0; jumpSourceAt = 0; jumpSourceLiving = false;
            jumpLastGrounded = jumpDamageAt = -1; jumpAwaitingSource = false;
        }
    }
    public void tickJump(boolean active, long generation, int mode, int chance, int delay, boolean onlyCombat)
        throws ReflectiveOperationException {
        Object c = client(); if (c == null) return;
        if (!active) { cancelJump(); return; }
        prepare(5);
        try {
            if (jumpFailed || mode < 0 || mode > 1 || chance < 0 || chance > 100 || delay < 0 || delay > 100 ||
                chance == 0 || !jumpPermitted(generation)) { cancelJump(); return; }
            if (generation != jumpGeneration || mode != jumpMode || chance != jumpChance || delay != jumpDelay ||
                onlyCombat != jumpOnlyCombat) {
                cancelJump(); jumpGeneration = generation; jumpMode = mode; jumpChance = chance;
                jumpDelay = delay; jumpOnlyCombat = onlyCombat;
            }
            if (!jumpHolding && !releaseJump()) return;
            Object p = player.get(c), w = world.get(c), opts = options.get(c);
            long handle = jumpWindow(c);
            if (handle == 0 || !jumpPlayable(c, p, w) || opts == null) { cancelJump(); return; }
            Object key = jumpKey.get(opts), input = key == null ? null : jumpBound.get(key);
            if (jumpDescriptor(input) == null || (mode == 1 && !jumpSpace(input))) { cancelJump(); return; }
            long now = System.nanoTime() / 1_000_000L;
            int hurt = jumpHurt.getInt(p), age = jumpAge.getInt(p);
            boolean grounded = (boolean) jumpGround.invoke(p);
            if (age < 0 || hurt < 0) { cancelJump(); return; }
            if (!jumpSeeded || p != jumpPlayer || w != jumpWorld || opts != jumpOptions || key != jumpBinding || input != jumpInput || handle != jumpWindowHandle) {
                cancelJump(); if (!releaseJump()) return;
                jumpPlayer = p; jumpWorld = w; jumpOptions = opts; jumpBinding = key; jumpInput = input;
                jumpWindowHandle = handle; jumpPreviousHurt = hurt; jumpLastGrounded = grounded ? now : -1;
                jumpPreviousSource = jumpSource.invoke(p); jumpSeeded = true; return;
            }
            boolean damaged = hurt > 0 && hurt > jumpPreviousHurt; jumpPreviousHurt = hurt;
            if (grounded) jumpLastGrounded = now;
            Object source = jumpSource.invoke(p);
            if (source != null && source != jumpPreviousSource) {
                Object attacker = jumpAttacker.invoke(source);
                jumpSourceLiving = attacker != p && jumpLiving.isInstance(attacker);
                jumpPreviousSource = source; ++jumpSourceRevision; jumpSourceAt = now;
            }
            boolean livingDamage = source != null && jumpSourceLiving && jumpSourceRevision != jumpConsumedSource && now - jumpSourceAt <= 105;
            boolean physical = jumpPhysical(input, handle);
            if (jumpHolding) {
                if (physical || age > jumpSentAge || now >= jumpHoldUntil || age < jumpSentAge) {
                    jumpHolding = false; releaseJump();
                }
                return;
            }
            if (physical || (boolean) jumpPressed.invoke(key)) { jumpPending = false; return; }
            if (damaged) {
                jumpDamageAt = now; jumpAwaitingSource = !livingDamage;
                jumpPending = false;
                if (now >= jumpNextAllowed && (grounded || (jumpLastGrounded >= 0 && now - jumpLastGrounded <= 85)) &&
                    (chance == 100 || java.util.concurrent.ThreadLocalRandom.current().nextInt(1, 101) <= chance)) {
                    jumpPending = true; jumpConfirmed = !onlyCombat || livingDamage;
                    jumpConfirmUntil = now + 105; jumpReadyAt = now + delay; jumpExpiresAt = now + 170;
                }
            }
            if (livingDamage && (damaged || (jumpPending && jumpConfirmed) ||
                (jumpAwaitingSource && jumpDamageAt >= 0 && now - jumpDamageAt <= 105))) {
                jumpConsumedSource = jumpSourceRevision; jumpAwaitingSource = false;
            }
            if (!jumpPending) return;
            if (now >= jumpExpiresAt || hurt == 0) { jumpPending = false; return; }
            if (!jumpConfirmed) {
                if (now >= jumpConfirmUntil) { jumpPending = false; return; }
                jumpConfirmed = livingDamage; if (!jumpConfirmed) return;
            }
            if (now < jumpReadyAt || !grounded || jumpCooldown.getInt(p) != 0) return;
            // Final client-thread revalidation; consume this hit before mutation.
            if (client() != c || player.get(c) != p || world.get(c) != w || options.get(c) != opts ||
                jumpKey.get(opts) != key || jumpBound.get(key) != input || !jumpPlayable(c, p, w) ||
                !(boolean) jumpGround.invoke(p) || jumpCooldown.getInt(p) != 0 || jumpWindow(c) != handle || jumpPhysical(input, handle) ||
                (boolean) jumpPressed.invoke(key) || !jumpPermitted(generation)) {
                jumpPending = false; return;
            }
            jumpPending = false; jumpHolding = true; jumpSentAge = age;
            jumpHoldUntil = now + 200; jumpNextAllowed = now + 140;
            jumpOwner = new Object[]{opts, key, input, handle};
            jumpSetPressed.invoke(key, true);
        } catch (ReflectiveOperationException | RuntimeException error) {
            jumpFailed = true;
            try { cancelJump(); } catch (ReflectiveOperationException cleanup) { error.addSuppressed(cleanup); }
            throw error;
        }
    }

    private Class<?> tickEvents;
    private Method stopTickEvents;
    public void installTickEvents() throws ReflectiveOperationException {
        if (tickEvents != null) return;
        Class<?> events=Class.forName("net.onestep.hybrid.ClientTickRelay",true,loader);
        Method install=events.getMethod("install",Object.class,java.util.function.Consumer.class);
        stopTickEvents=events.getMethod("stop",Object.class);
        install.invoke(null,this,(java.util.function.Consumer<Object>)this::onTotemTick);
        tickEvents=events;
    }

    // Feature 7: Java port of Argon's AutoInventoryTotem tick flow.
    protected native boolean totemPermitted(long generation);
    private Class<?> totemScreenType;
    private Constructor<?> totemScreenCtor;
    private Method totemInventory, totemStack, totemOffhand, totemMainHand, totemEmpty, totemItem;
    private Method totemClick, totemSetScreen, totemClose;
    private Field totemHandler, totemScreenHandler, totemSync, totemSelected;
    private Object totemValue, totemSwap, totemPlayer, totemWorld, totemManager, totemInventoryOwner;
    private Object totemOpenedScreen;
    private int totemClock=-1, totemCloseClock=-1;
    private volatile boolean totemActive;
    private boolean totemFailed;
    private long totemGeneration;
    private int[] totemValues;
    private boolean totemAutoOpen;
    private final java.util.Random totemRandom=new java.util.Random();

    private void prepareTotem() throws ReflectiveOperationException {
        Class<?> inv=load("class_1661"), stack=load("class_1799"), itemType=load("class_1792");
        Class<?> handler=load("class_1703"), action=load("class_1713"), screenType=load("class_437");
        totemScreenType=load("class_490");
        Class<?> playerBase=load("class_1657");
        Class<?> fake;
        try { fake=Class.forName("net.onestep.hybrid.TotemScreen",false,WorldBridge.class.getClassLoader()); }
        catch (ClassNotFoundException missing) {
            Class<?> transformer=Class.forName("net.onestep.hybrid.ClientTickTransformer",true,WorldBridge.class.getClassLoader());
            Method factory=transformer.getMethod("inventoryScreen",Class.class,Class.class,String.class,String.class);
            byte[] bytes=(byte[])factory.invoke(null,totemScreenType,playerBase,name("method_2383"),name("method_25402"));
            try { fake=java.lang.invoke.MethodHandles.lookup().defineClass(bytes); }
            catch (IllegalAccessException error) { throw new ReflectiveOperationException("Totem screen class",error); }
        }
        totemScreenCtor=constructor(fake,playerBase);
        totemInventory=mappedMethod(playerType,"method_31548",inv);
        totemStack=mappedMethod(inv,"method_5438",stack,int.class);
        totemOffhand=mappedMethod(playerType,"method_6079",stack);
        totemMainHand=mappedMethod(playerType,"method_6047",stack);
        totemEmpty=mappedMethod(stack,"method_7960",boolean.class);
        totemItem=mappedMethod(stack,"method_7909",itemType);
        totemValue=mappedField(load("class_1802"),"field_8288",itemType,true).get(null);
        totemSwap=mappedField(action,"field_7791",action,true).get(null);
        totemHandler=mappedField(playerType,"field_7512",handler,false);
        totemScreenHandler=mappedField(load("class_465"),"field_2797",handler,false);
        totemSync=mappedField(handler,"field_7763",int.class,false);
        totemSelected=mappedField(inv,"field_7545",int.class,false);
        totemClick=mappedMethod(load("class_636"),"method_2906",void.class,int.class,int.class,int.class,action,load("class_1657"));
        totemSetScreen=mappedMethod(clientType,"method_1507",void.class,screenType);
        totemClose=mappedMethod(screenType,"method_25419",void.class);
    }
    public void cancelTotem() {
        totemPlayer=totemWorld=totemManager=totemInventoryOwner=totemOpenedScreen=null;
        totemClock=totemCloseClock=-1; totemActive=false;
    }
    private boolean totemIs(Object stack) throws ReflectiveOperationException {
        return stack!=null && !(boolean)totemEmpty.invoke(stack) && totemItem.invoke(stack)==totemValue;
    }
    private int totemFind(Object inv,int mode) throws ReflectiveOperationException {
        java.util.ArrayList<Integer> slots=new java.util.ArrayList<>();
        for (int i=9;i<36;++i) if (totemIs(totemStack.invoke(inv,i))) {
            if (mode==0) return i;
            slots.add(i);
        }
        return slots.isEmpty() ? -1 : slots.get(totemRandom.nextInt(slots.size()));
    }
    private boolean totemHasOutside(Object inv) throws ReflectiveOperationException {
        return totemFind(inv,0)>=0;
    }
    private void totemClick(Object c,Object p,Object inv,Object h,int source,int destination) throws ReflectiveOperationException {
        if (source<9 || source>=36 || (destination!=40 && (destination<0 || destination>8))) return;
        int sync=totemSync.getInt(h);
        Object s=screen.get(c);
        if (player.get(c)!=p || world.get(c)!=totemWorld || manager.get(c)!=totemManager ||
            totemInventory.invoke(p)!=inv || totemHandler.get(p)!=h || !totemScreenType.isInstance(s) ||
            totemScreenHandler.get(s)!=h || !totemIs(totemStack.invoke(inv,source)) || !totemPermitted(totemGeneration)) return;
        totemClick.invoke(manager.get(c),sync,source,destination,totemSwap,p);
    }
    /** Argon AutoInventoryTotem settings: mode, delay ticks, hotbar, slot, auto switch, force, stay-open ticks. */
    public void tickTotem(boolean active,long generation,int[] values,boolean autoOpen) throws ReflectiveOperationException {
        if (!active || stopped) { cancelTotem(); return; }
        if (values==null || values.length!=7 || values[0]<0 || values[0]>1) throw new IllegalArgumentException("Auto Totem settings");
        prepare(7);
        if (client()==null) return;
        totemGeneration=generation; totemValues=values.clone(); totemAutoOpen=autoOpen;
        if (tickEvents==null) throw new IllegalStateException("Auto Totem requires tick hooks");
        totemActive=!totemFailed && totemPermitted(generation);
    }
    private void onTotemTick(Object c) {
        if (!totemActive || stopped) return;
        try { runTotemTick(c); }
        catch (ReflectiveOperationException | RuntimeException error) { totemFailed=true; cancelTotem(); }
    }
    private void runTotemTick(Object c) throws ReflectiveOperationException {
        if (c==null || client()!=c) return;
        int[] values=totemValues; boolean autoOpen=totemAutoOpen; long generation=totemGeneration;
        Object p=player.get(c),w=world.get(c),m=manager.get(c);
        if (p==null || w==null || m==null || !(boolean)alive.invoke(p) || !totemPermitted(generation)) { cancelTotem(); return; }
        Object inv=totemInventory.invoke(p),s=screen.get(c),h=totemHandler.get(p);
        if (inv==null || h==null) { cancelTotem(); return; }
        if (totemPlayer!=p || totemWorld!=w || totemManager!=m || totemInventoryOwner!=inv) {
            totemClock=totemCloseClock=-1; totemOpenedScreen=null;
            totemPlayer=p; totemWorld=w; totemManager=m; totemInventoryOwner=inv;
        }
        boolean screenIsOwned=s!=null && s==totemOpenedScreen;
        boolean invScreen=s!=null && totemScreenType.isInstance(s);
        boolean offhand=totemIs(totemOffhand.invoke(p));
        int desired=Math.max(1,Math.min(9,values[3]))-1;
        boolean hotbar=values[2]!=0;
        boolean desiredHotbar=totemIs(totemStack.invoke(inv,desired));
        boolean shouldOpen=(hotbar ? (!offhand || !desiredHotbar) : !offhand) && !screenIsOwned && totemHasOutside(inv);
        if (autoOpen && shouldOpen) {
            Object fake=totemScreenCtor.newInstance(p);
            if (!totemPermitted(generation)) return;
            totemSetScreen.invoke(c,fake);
            if (screen.get(c)==fake) {
                totemOpenedScreen=fake;
                // Continue this same tick through the local inventory screen.
                s=fake; screenIsOwned=true; invScreen=true;
            }
        }
        if (!invScreen) { totemClock=-1; totemCloseClock=-1; totemOpenedScreen=null; return; }
        if (totemScreenHandler.get(s)!=totemHandler.get(p)) return;
        int delay=Math.max(0,Math.min(20,values[1]));
        int stay=Math.max(0,Math.min(20,values[6]));
        if (totemClock==-1) totemClock=delay;
        if (totemCloseClock==-1) totemCloseClock=stay;
        if (totemClock>0) totemClock--;
        if (values[4]!=0) totemSelected.setInt(inv,desired);
        h=totemHandler.get(p);
        if (h==null) return;
        if (totemClock<=0) {
            offhand=totemIs(totemOffhand.invoke(p));
            if (!offhand) {
                int source=totemFind(inv,values[0]);
                if (source!=-1) { totemClick(c,p,inv,h,source,40); return; }
            }
            if (hotbar) {
                Object hand=totemMainHand.invoke(p);
                boolean empty=(boolean)totemEmpty.invoke(hand), already=totemIs(hand);
                if (empty || (values[5]!=0 && !already)) {
                    int source=totemFind(inv,values[0]);
                    if (source!=-1) {
                        int selected=totemSelected.getInt(inv);
                        totemClick(c,p,inv,h,source,selected); return;
                    }
                }
            }
            boolean close=screenIsOwned && (hotbar ? totemIs(totemStack.invoke(inv,desired)) && totemIs(totemOffhand.invoke(p)) : totemIs(totemOffhand.invoke(p)));
            if (close && autoOpen) {
                if (totemCloseClock!=0) { totemCloseClock--; return; }
                if (screen.get(c)==s && totemPermitted(generation)) totemClose.invoke(s);
                totemOpenedScreen=null; totemCloseClock=stay;
            }
        }
    }

    public synchronized void stop() {
        cancelTotem();
        if (tickEvents != null) try { stopTickEvents.invoke(null,this); } catch (ReflectiveOperationException ignored) {}
        tickEvents=null;
        jumpSeeded = jumpHolding = jumpPending = false;
        jumpPlayer = jumpWorld = jumpOptions = jumpBinding = jumpInput = jumpPreviousSource = null;
        jumpOwner=null;
        triggerActive=false;
        triggerPlayer=triggerWorld=triggerManager=triggerTarget=triggerInv=null;
        stopped=true; pearlOwner=null; pearlNonce=0;
    }
}
