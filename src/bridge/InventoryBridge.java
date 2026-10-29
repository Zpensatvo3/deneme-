package net.onestep.hybrid;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Client-thread inventory I/O for OneStep's Java-backed inventory features. */
public final class InventoryBridge {
    public static final int REFILL = 4, MOUSE = 5;
    private final ClassLoader loader;
    private final Class<?> playerType;
    private final Field instance, player, world, screen, manager, currentHandler;
    private final Method onThread, alive, getWindow, getHandle, glfwGetMouseButton, glfwGetKey;
    private final boolean[] prepared = new boolean[6];
    private final long[] nonce = new long[6];
    private Object[] refill;
    private Field focused, slotInventory, slotIndex, swapAction, potionComponent, healing, strongHealing;
    private Method playerInventory, enabled, insert, limit, maxCount, component, matches;
    private Class<?> creativeScreen, potionItem;
    private Field handler, slots, slotId, syncId, pickup, quickMove;
    private Method getStack, cursor, empty, copy, count, same, item, click;
    private Class<?> handled;
    private long sequence;
    private volatile boolean stopped;
    private Object mousePlayer, mouseWorld, mouseScreen, mouseHandler;
    private Object mouseWindow;
    private long mouseWindowHandle;
    private int mouseSync = -1, mouseRightSlot = -1, mouseLeftSlot = -1;
    private boolean mouseRightActive, mouseLeftActive, mouseLeftOriginal;
    private boolean mouseLastRight, mouseLastLeft, mouseCursorWasEmpty = true, mouseCursorKnown;
    private Method isSprinting, setSprinting, flag, setFlag;
    private Field inputField, inputFlagsField, movementForward, movementSideways, jumpingField;
    private Field networkField;
    private Method sendPacketMethod;
    private java.lang.reflect.Constructor<?> packetConstructor;
    private java.lang.reflect.Constructor<?> stopSprintingConstructor, stopSprintingConstructor3;
    private Object stopSprintingMode;
    private Object emptyInput;

    public InventoryBridge(ClassLoader loader, int target) throws ReflectiveOperationException {
        if (loader == null || target != 12111) throw new IllegalArgumentException("InventoryBridge requires Fabric 1.21.11");
        this.loader = loader;
        Class<?> client = load("class_310"); playerType = load("class_746");
        instance = field(client, "field_1700", client, true);
        player = field(client, "field_1724", playerType, false);
        world = field(client, "field_1687", load("class_638"), false);
        screen = field(client, "field_1755", load("class_437"), false);
        manager = field(client, "field_1761", load("class_636"), false);
        currentHandler = field(playerType, "field_7512", load("class_1703"), false);
        onThread = method(client, "method_18854", boolean.class, false);
        alive = method(playerType, "method_5805", boolean.class, false);
        getWindow = method(client, "method_22683", load("class_1041"), false);
        getHandle = method(load("class_1041"), "method_4490", long.class, false);
        Class<?> glfw = Class.forName("org.lwjgl.glfw.GLFW", false, loader);
        glfwGetMouseButton = method(glfw, "glfwGetMouseButton", int.class, true, long.class, int.class);
        glfwGetKey = method(glfw, "glfwGetKey", int.class, true, long.class, int.class);
        initGrimSafety();
    }
    private Class<?> load(String name) throws ClassNotFoundException {
        return Class.forName("net.minecraft." + name, false, loader);
    }
    private static Field field(Class<?> owner, String name, Class<?> type, boolean isStatic) throws ReflectiveOperationException {
        for (Class<?> c = owner; c != null; c = c.getSuperclass()) try {
            Field f = c.getDeclaredField(name);
            if (f.getType() != type || Modifier.isStatic(f.getModifiers()) != isStatic || !f.trySetAccessible())
                throw new IllegalAccessException("InventoryBridge field mismatch: " + name);
            return f;
        } catch (NoSuchFieldException missing) { /* Exact inherited member only. */ }
        throw new NoSuchFieldException(name);
    }
    private static Method method(Class<?> owner, String name, Class<?> result, boolean isStatic, Class<?>... args)
            throws ReflectiveOperationException {
        for (Class<?> c = owner; c != null; c = c.getSuperclass()) try {
            Method m = c.getDeclaredMethod(name, args);
            if (m.getReturnType() != result || Modifier.isStatic(m.getModifiers()) != isStatic || !m.trySetAccessible())
                throw new IllegalAccessException("InventoryBridge method mismatch: " + name);
            return m;
        } catch (NoSuchMethodException missing) { /* Exact inherited member only. */ }
        throw new NoSuchMethodException(name);
    }
    private static void feature(int f) { if (f != REFILL && f != MOUSE) throw new IllegalArgumentException("Inventory feature"); }
    public void prepare(int f) throws ReflectiveOperationException {
        feature(f);
        if (stopped) throw new IllegalStateException("InventoryBridge stopped");
        if (prepared[f]) return;
        Class<?> h = load("class_1703"), slot = load("class_1735"), stack = load("class_1799"), action = load("class_1713");
        handled = load("class_465");
        handler = field(handled, "field_2797", h, false);
        slots = field(h, "field_7761", load("class_2371"), false);
        syncId = field(h, "field_7763", int.class, false);
        slotId = field(slot, "field_7874", int.class, false);
        getStack = method(slot, "method_7677", stack, false);
        cursor = method(h, "method_34255", stack, false);
        empty = method(stack, "method_7960", boolean.class, false);
        copy = method(stack, "method_7972", stack, false);
        count = method(stack, "method_7947", int.class, false);
        same = method(stack, "method_31577", boolean.class, true, stack, stack);
        click = method(load("class_636"), "method_2906", void.class, false,
            int.class, int.class, int.class, action, load("class_1657"));
            focused = field(handled, "field_2787", slot, false);
            playerInventory = method(playerType, "method_31548", load("class_1661"), false);
            slotInventory = field(slot, "field_7871", load("class_1263"), false);
            slotIndex = field(slot, "field_7875", int.class, false);
            enabled = method(slot, "method_7682", boolean.class, false);
            insert = method(slot, "method_7680", boolean.class, false, stack);
            limit = method(slot, "method_7676", int.class, false, stack);
            maxCount = method(stack, "method_7914", int.class, false);
            pickup = field(action, "field_7790", action, true);
            quickMove = field(action, "field_7794", action, true);
            if (f == REFILL) {
                creativeScreen = load("class_481"); potionItem = load("class_1812");
                swapAction = field(action, "field_7791", action, true);
                item = method(stack, "method_7909", load("class_1792"), false);
                potionComponent = field(load("class_9334"), "field_49651", load("class_9331"), true);
                // get() is inherited from an interface, not necessarily declared by ItemStack.
                component = method(load("class_9473"), "method_58694", Object.class, false, load("class_9331"));
                matches = method(load("class_1844"), "method_57401", boolean.class, false, load("class_6880"));
                healing = field(load("class_1847"), "field_8963", load("class_6880"), true);
                strongHealing = field(load("class_1847"), "field_8980", load("class_6880"), true);
            }
        prepared[f] = true;
    }
    private Object client() throws ReflectiveOperationException {
        if (stopped) return null;
        Object c = instance.get(null);
        return c != null && (boolean) onThread.invoke(c) ? c : null;
    }
    private void resetMouse() {
        mousePlayer = mouseWorld = mouseScreen = mouseHandler = null;
        mouseSync = mouseRightSlot = mouseLeftSlot = -1;
        mouseRightActive = mouseLeftActive = mouseLeftOriginal = false;
        mouseLastRight = mouseLastLeft = false;
        mouseCursorWasEmpty = true; mouseCursorKnown = false;
    }
    private static boolean held(int state) { return state == 1 || state == 2; }
    private boolean mouseStillCurrent(Object p, Object w, Object s, Object h, int sync)
            throws ReflectiveOperationException {
        return mousePlayer == p && mouseWorld == w && mouseScreen == s && mouseHandler == h && mouseSync == sync;
    }
    private boolean mouseSlotCanTakeOne(Object slot, Object carried) throws ReflectiveOperationException {
        if (slot == null || !(boolean) enabled.invoke(slot) || !(boolean) insert.invoke(slot, carried)) return false;
        Object target = getStack.invoke(slot);
        if (target == null) return false;
        if ((boolean) empty.invoke(target)) return true;
        if (!(boolean) same.invoke(null, target, carried)) return false;
        return (int) count.invoke(target) < Math.min((int) limit.invoke(slot, carried), (int) maxCount.invoke(carried));
    }
    private void initGrimSafety() {
        try {
            Class<?> entity = load("class_1297");
            isSprinting = method(playerType, "method_5624", boolean.class, false);
            setSprinting = method(playerType, "method_5728", void.class, false, boolean.class);
            flag = method(entity, "method_5795", boolean.class, false, int.class);
            setFlag = method(entity, "method_5729", void.class, false, int.class, boolean.class);
        } catch (Throwable ignored) {}

        try {
            Class<?> inputType = load("class_744");
            Class<?> flagsType = load("class_10185");
            inputField = field(playerType, "field_3913", inputType, false);
            inputFlagsField = field(inputType, "field_54155", flagsType, false);
            try { movementForward = field(inputType, "field_3905", float.class, false); } catch (Throwable ignored) {}
            try { movementSideways = field(inputType, "field_3907", float.class, false); } catch (Throwable ignored) {}
            try { jumpingField = field(inputType, "field_3904", boolean.class, false); } catch (Throwable ignored) {}

            try {
                Field emptyField = flagsType.getDeclaredField("field_54098");
                emptyField.setAccessible(true);
                emptyInput = emptyField.get(null);
            } catch (Throwable ignored) {}

            if (emptyInput == null) {
                for (java.lang.reflect.Constructor<?> ctor : flagsType.getDeclaredConstructors()) {
                    if (ctor.getParameterCount() == 7) {
                        ctor.setAccessible(true);
                        emptyInput = ctor.newInstance(false, false, false, false, false, false, false);
                        break;
                    }
                }
            }

            Class<?> networkType = load("class_634");
            Class<?> inputPacketClass = load("class_2851");
            networkField = field(playerType, "field_3944", networkType, false);
            for (Class<?> c = networkType; c != null; c = c.getSuperclass()) {
                for (Method m : c.getDeclaredMethods()) {
                    if ((m.getName().equals("method_2883") || m.getName().equals("sendPacket")) && m.getParameterCount() == 1) {
                        m.setAccessible(true);
                        sendPacketMethod = m;
                        break;
                    }
                }
                if (sendPacketMethod != null) break;
            }
            for (java.lang.reflect.Constructor<?> ctor : inputPacketClass.getDeclaredConstructors()) {
                if (ctor.getParameterCount() == 1) {
                    ctor.setAccessible(true);
                    packetConstructor = ctor;
                    break;
                }
            }

            Class<?> commandClass = load("class_2848");
            Class<?> commandModeClass = load("class_2848$class_2849");
            if (commandClass != null && commandModeClass != null) {
                for (Object constant : commandModeClass.getEnumConstants()) {
                    if ("STOP_SPRINTING".equals(constant.toString()) || ((Enum<?>) constant).name().equals("field_12985")) {
                        stopSprintingMode = constant;
                        break;
                    }
                }
                for (java.lang.reflect.Constructor<?> ctor : commandClass.getDeclaredConstructors()) {
                    if (ctor.getParameterCount() == 2 && ctor.getParameterTypes()[1] == commandModeClass) {
                        ctor.setAccessible(true);
                        stopSprintingConstructor = ctor;
                        break;
                    } else if (ctor.getParameterCount() == 3 && ctor.getParameterTypes()[1] == commandModeClass) {
                        ctor.setAccessible(true);
                        stopSprintingConstructor3 = ctor;
                    }
                }
            }
        } catch (Throwable ignored) {}
    }
    private void suppressInput(Object p) {
        if (p == null) return;
        try {
            if (inputField != null) {
                Object in = inputField.get(p);
                if (in != null) {
                    if (inputFlagsField != null && emptyInput != null) {
                        inputFlagsField.set(in, emptyInput);
                    }
                    if (movementForward != null) movementForward.setFloat(in, 0.0f);
                    if (movementSideways != null) movementSideways.setFloat(in, 0.0f);
                    if (jumpingField != null) jumpingField.setBoolean(in, false);
                }
            }
        } catch (Throwable ignored) {}
    }
    private void sendStopSprinting(Object p) {
        if (p == null || networkField == null || sendPacketMethod == null || stopSprintingMode == null) return;
        try {
            Object net = networkField.get(p);
            if (net != null) {
                Object packet = null;
                if (stopSprintingConstructor != null) {
                    packet = stopSprintingConstructor.newInstance(p, stopSprintingMode);
                } else if (stopSprintingConstructor3 != null) {
                    packet = stopSprintingConstructor3.newInstance(p, stopSprintingMode, 0);
                }
                if (packet != null) {
                    sendPacketMethod.invoke(net, packet);
                }
            }
        } catch (Throwable ignored) {}
    }

    private void sendNeutralInput(Object p) {
        if (p == null || networkField == null || sendPacketMethod == null || packetConstructor == null || emptyInput == null) return;
        try {
            Object net = networkField.get(p);
            if (net != null) {
                Object packet = packetConstructor.newInstance(emptyInput);
                sendPacketMethod.invoke(net, packet);
            }
        } catch (Throwable ignored) {}
    }

    private void safeClick(Object interaction, Object p, int sync, int slot, int button, Object action)
            throws ReflectiveOperationException {
        if (interaction == null || p == null || slot < 0) return;
        try {
            if (isSprinting != null && (boolean) isSprinting.invoke(p)) {
                setSprinting.invoke(p, false);
                sendStopSprinting(p);
            }
            if (flag != null && (boolean) flag.invoke(p, 3)) {
                setFlag.invoke(p, 3, false);
            }
            suppressInput(p);
            sendNeutralInput(p);
        } catch (Throwable ignored) {}
        click.invoke(interaction, sync, slot, button, action, p);
    }
    private void mouseClick(Object interaction, Object p, int sync, int slot, int button, Object action)
            throws ReflectiveOperationException {
        if (slot >= 0) safeClick(interaction, p, sync, slot, button, action);
    }
    /** Called once per rendered frame; all gesture decisions and slot reads stay Java-side. */
    public void tickMouse(boolean active, boolean rightTweak, boolean leftWithItem, boolean leftWithoutItem)
            throws ReflectiveOperationException {
        if (!active || stopped) { resetMouse(); return; }
        prepare(MOUSE);
        Object c = client();
        if (c == null) { resetMouse(); return; }
        Object p = player.get(c), w = world.get(c), s = screen.get(c), m = manager.get(c);
        if (p == null || w == null || m == null || !(boolean) alive.invoke(p) || !handled.isInstance(s)) {
            resetMouse(); return;
        }
        Object h = handler.get(s);
        if (h == null || h != currentHandler.get(p)) { resetMouse(); return; }
        int sync = syncId.getInt(h);
        if (!mouseStillCurrent(p, w, s, h, sync)) {
            resetMouse(); mousePlayer = p; mouseWorld = w; mouseScreen = s; mouseHandler = h; mouseSync = sync;
        }
        Object window = getWindow.invoke(c);
        if (window == null) { resetMouse(); return; }
        if (window != mouseWindow || mouseWindowHandle == 0) {
            mouseWindow = window;
            mouseWindowHandle = (long) getHandle.invoke(window);
        }
        long handle = mouseWindowHandle;
        boolean right = held((int) glfwGetMouseButton.invoke(null, handle, 1));
        boolean left = held((int) glfwGetMouseButton.invoke(null, handle, 0));
        boolean shift = held((int) glfwGetKey.invoke(null, handle, 340)) || held((int) glfwGetKey.invoke(null, handle, 344));
        Object carried = cursor.invoke(h);
        if (carried == null) { resetMouse(); return; }
        boolean cursorEmpty = (boolean) empty.invoke(carried);
        Object hovered = focused.get(s);
        List<?> list = (List<?>) slots.get(h);
        int hover = hovered == null ? -1 : slotId.getInt(hovered);
        if (list == null || list.isEmpty() || list.size() > 4096 ||
            (hovered != null && (hover < 0 || hover >= list.size() || list.get(hover) != hovered))) {
            resetMouse(); return;
        }
        if (!mouseCursorKnown) { mouseCursorWasEmpty = cursorEmpty; mouseCursorKnown = true; }
        boolean rightPressed = right && !mouseLastRight, leftPressed = left && !mouseLastLeft;
        if (!rightTweak || !right) {
            mouseRightActive = false; mouseRightSlot = -1; mouseLeftOriginal = false;
        } else if (rightPressed && !mouseCursorWasEmpty) {
            mouseRightActive = true; mouseRightSlot = hover; mouseLeftOriginal = false;
        }
        if ((!leftWithItem && !leftWithoutItem) || !left) {
            mouseLeftActive = false; mouseLeftSlot = -1;
        } else if (leftPressed && mouseCursorWasEmpty) {
            mouseLeftActive = true; mouseLeftSlot = hover;
        }
        if (mouseRightActive && hover != mouseRightSlot) {
            if (!mouseLeftOriginal) {
                mouseLeftOriginal = true;
                if (mouseRightSlot >= 0 && !cursorEmpty && mouseRightSlot < list.size()) {
                    Object original = list.get(mouseRightSlot);
                    if (mouseSlotCanTakeOne(original, carried)) mouseClick(m, p, sync, mouseRightSlot, 1, pickup.get(null));
                }
            }
            mouseRightSlot = hover;
            carried = cursor.invoke(h);
            if (hovered != null && carried != null && !((boolean) empty.invoke(carried)) && mouseSlotCanTakeOne(hovered, carried))
                mouseClick(m, p, sync, hover, 1, pickup.get(null));
        }
        if (mouseLeftActive && hover != mouseLeftSlot) {
            mouseLeftSlot = hover;
            if (hovered != null && (boolean) enabled.invoke(hovered)) {
                Object target = getStack.invoke(hovered);
                if (target != null && !((boolean) empty.invoke(target))) {
                    carried = cursor.invoke(h);
                    cursorEmpty = carried == null || (boolean) empty.invoke(carried);
                    if (cursorEmpty) {
                        if (shift && leftWithoutItem) mouseClick(m, p, sync, hover, 0, quickMove.get(null));
                    } else if (leftWithItem && (boolean) same.invoke(null, target, carried)) {
                        if (shift) mouseClick(m, p, sync, hover, 0, quickMove.get(null));
                        else {
                            int cursorCount = (int) count.invoke(carried), targetCount = (int) count.invoke(target);
                            int cursorLimit = (int) maxCount.invoke(carried);
                            if (cursorCount > 0 && targetCount > 0 && cursorCount + targetCount <= cursorLimit) {
                                boolean output = !(boolean) insert.invoke(hovered, carried);
                                mouseClick(m, p, sync, hover, 0, pickup.get(null));
                                if (!output) {
                                    Object afterCursor = cursor.invoke(h), afterTarget = getStack.invoke(hovered);
                                    if (afterCursor != null && afterTarget != null && (boolean) empty.invoke(afterCursor) &&
                                        (boolean) same.invoke(null, afterTarget, target) &&
                                        (int) count.invoke(afterTarget) == cursorCount + targetCount)
                                        mouseClick(m, p, sync, hover, 0, pickup.get(null));
                                }
                            }
                        }
                    }
                }
            }
        }
        mouseLastRight = right; mouseLastLeft = left; mouseCursorWasEmpty = cursorEmpty;
    }
    private boolean equalStack(Object a, Object b) throws ReflectiveOperationException {
        return a != null && b != null && (int) count.invoke(a) == (int) count.invoke(b) && (boolean) same.invoke(null, a, b);
    }
    private boolean health(Object stack) throws ReflectiveOperationException {
        if ((boolean) empty.invoke(stack) || !potionItem.isInstance(item.invoke(stack))) return false;
        Object contents = component.invoke(stack, potionComponent.get(null));
        return contents != null && ((boolean) matches.invoke(contents, healing.get(null)) ||
            (boolean) matches.invoke(contents, strongHealing.get(null)));
    }
    private boolean context(Object[] a, Object[] b) {
        for (int i = 1; i <= 6; ++i) if (a[i] != b[i]) return false;
        return ((double[]) a[10])[3] == ((double[]) b[10])[3];
    }
    /** {owner,p,w,screen,handler,manager,inventory,slots[],copies[],cursorCopy,double[10+9*n]}. */
    public Object[] captureContainer(int f) throws ReflectiveOperationException {
        if (f != REFILL) throw new IllegalArgumentException("Container feature");
        Object c = client(); if (c == null) return null;
        nonce[f] = 0; prepare(f);
        Object p = player.get(c), w = world.get(c), s = screen.get(c), m = manager.get(c);
        if (p == null || w == null || m == null || !(boolean) alive.invoke(p) || !handled.isInstance(s) ||
            creativeScreen.isInstance(s)) return null;
        Object h = handler.get(s), inv = playerInventory.invoke(p);
        if (h == null || h != currentHandler.get(p) || inv == null || !(slots.get(h) instanceof List<?> list) ||
            list.isEmpty() || list.size() > 4096) return null;
        Object carried = copy.invoke(cursor.invoke(h));
        if (carried == null) return null;
        boolean cursorEmpty = (boolean) empty.invoke(carried);
        Object[] objects = list.toArray(), copies = new Object[objects.length];
        double[] data = new double[10 + 9 * objects.length];
        Object hovered = focused.get(s);
        int hover = hovered == null ? -1 : slotId.getInt(hovered);
        if (hovered != null && (hover < 0 || hover >= objects.length || objects[hover] != hovered)) return null;
        data[0] = 1; data[1] = f; data[2] = nonce[f] = ++sequence; data[3] = syncId.getInt(h);
        data[4] = hover; data[5] = cursorEmpty ? 1 : 0;
        data[6] = (int) count.invoke(carried); data[7] = (int) maxCount.invoke(carried); data[8] = objects.length;
        for (int i = 0; i < objects.length; ++i) {
            Object slot = objects[i];
            if (slot == null || slotId.getInt(slot) != i) return null;
            Object stack = copies[i] = copy.invoke(getStack.invoke(slot));
            if (stack == null) return null;
            boolean isEmpty = (boolean) empty.invoke(stack);
            int offset = 10 + i * 9;
            data[offset] = i;
            data[offset + 1] = slotInventory.get(slot) == inv ? slotIndex.getInt(slot) : -1;
            data[offset + 2] = (int) count.invoke(stack); data[offset + 3] = isEmpty ? 1 : 0;
            data[offset + 4] = !isEmpty && health(stack) ? 1 : 0;
            data[offset + 5] = (boolean) enabled.invoke(slot) ? 1 : 0;
            data[offset + 6] = !cursorEmpty && (boolean) insert.invoke(slot, carried) ? 1 : 0;
            data[offset + 7] = cursorEmpty ? 0 : (int) limit.invoke(slot, carried);
            data[offset + 8] = isEmpty || cursorEmpty || (boolean) same.invoke(null, stack, carried) ? 1 : 0;
        }
        if (refill != null && f == REFILL) {
            int[] indices = (int[]) refill[9];
            boolean sameContext = refill[0] == p && refill[1] == w && refill[2] == s && refill[3] == h &&
                refill[4] == m && refill[5] == inv && indices[2] == (int) data[3];
            if (!sameContext) refill = null;
            else if (indices[0] < objects.length && indices[1] < objects.length &&
                refill[6] == objects[indices[0]] && refill[7] == objects[indices[1]] &&
                (boolean) empty.invoke(copies[indices[0]]) && equalStack(copies[indices[1]], refill[8])) data[9] = 1;
        }
        if (sequence >= (1L << 53)) { stop(); return null; }
        return new Object[]{this, p, w, s, h, m, inv, objects, copies, carried, data};
    }
    /** The refill bridge's only container action is an explicitly validated hotbar SWAP. */
    public boolean containerAct(Object[] packet, int action, int source, int target,
                                boolean left, boolean right, boolean shift) throws ReflectiveOperationException {
        if (client() == null || packet == null || packet.length != 11 || packet[0] != this ||
            !(packet[10] instanceof double[] before) || before.length < 10 || before[0] != 1) return false;
        int f = (int) before[1];
        if (f != REFILL || before[2] <= 0 || before[2] != nonce[f]) return false;
        nonce[f] = 0;
        Object[] now = captureContainer(f); nonce[f] = 0;
        if (now == null || !context(packet, now) || !equalStack(packet[9], now[9])) return false;
        Object[] objects = (Object[]) now[7], stacks = (Object[]) now[8];
        Object[] oldObjects = (Object[]) packet[7], oldStacks = (Object[]) packet[8];
        if (source < 0 || source >= objects.length || source >= oldObjects.length || oldObjects[source] != objects[source] ||
            !equalStack(oldStacks[source], stacks[source])) return false;
        double[] data = (double[]) now[10]; int offset = 10 + source * 9;
        if (data[offset + 5] != 1) return false;
        if (action == 0) {
            if (left || right || refill != null || data[5] != 1 || data[offset + 1] < 9 ||
                data[offset + 1] >= 36 || data[offset + 4] != 1 || target < 0 || target >= objects.length ||
                target >= oldObjects.length || objects[target] != oldObjects[target] || !equalStack(stacks[target], oldStacks[target])) return false;
            int to = 10 + target * 9, index = (int) data[to + 1];
            if (index < 0 || index > 8 || data[to + 3] != 1 || data[to + 5] != 1 ||
                !(boolean) insert.invoke(objects[target], stacks[source])) return false;
            // Reject ambiguous custom layouts instead of guessing the hotbar address.
            for (int i = 0; i < objects.length; ++i) if (i != target && data[10 + i * 9 + 1] == index) return false;
            refill = new Object[]{now[1], now[2], now[3], now[4], now[5], now[6], objects[source], objects[target],
                copy.invoke(stacks[source]), new int[]{source, target, (int) data[3]}};
            safeClick(now[5], now[1], (int) data[3], source, index, swapAction.get(null));
            return true;
        }
        return false;
    }
    public void cancel(int f) {
        feature(f); nonce[f] = 0;
        if (f == REFILL) refill = null;
        if (f == MOUSE) resetMouse();
    }
    public void stop() { stopped = true; refill = null; resetMouse(); Arrays.fill(nonce, 0); }
}
