package net.minecraft;

import net.onestep.hybrid.GameBridge;

/** Deliberately tiny intermediary-named fixtures, not Minecraft/recovered code. */
public final class HybridBridgeTest {
    private static int checks;
    private static void check(boolean condition, String label) {
        ++checks;
        if (!condition) throw new AssertionError(label);
    }
    private static double[] data(Object[] packet) { return (double[]) packet[3]; }
    private static GameBridge bridge() throws Exception {
        return new GameBridge(HybridBridgeTest.class.getClassLoader(), 12111);
    }
    private static void expectBindingFailure(ClassLoader loader, int version) throws Exception {
        try { new GameBridge(loader, version); throw new AssertionError("Invalid binding accepted"); }
        catch (ReflectiveOperationException | IllegalArgumentException expected) { ++checks; }
    }
    public static void main(String[] args) throws Exception {
        class_310 client = new class_310();
        class_310.field_1700 = client;
        class_746 p = client.field_1724;
        GameBridge bridge = bridge();
        p.field_6228 = 7;
        Object[] jump = bridge.capture(1);
        check(jump[0] == bridge && jump[1] == p && jump[2] == client.field_1687, "snapshot identities");
        check(data(jump).length == 11 && data(jump)[0] == 1 && data(jump)[3] == 7, "wire schema");
        check(bridge.clearJump(jump) && p.field_6228 == 0, "jump clear");
        p.field_6228 = 6;
        check(!bridge.clearJump(jump) && p.field_6228 == 6, "single use");
        jump = bridge.capture(1);
        Object[] latest = bridge.capture(1);
        check(!bridge.clearJump(jump) && bridge.clearJump(latest), "new capture supersedes old");
        p.field_6228 = 5;
        jump = bridge.capture(1);
        p.field_6228 = 4;
        check(!bridge.clearJump(jump) && p.field_6228 == 4, "cooldown changed before write");
        jump = bridge.capture(1);
        client.field_1687 = new class_638();
        check(!bridge.clearJump(jump) && p.field_6228 == 4, "world changed before write");
        jump = bridge.capture(1);
        client.field_1724 = new class_746();
        check(!bridge.clearJump(jump) && p.field_6228 == 4, "player changed before write");
        client.field_1724 = p;
        jump = bridge.capture(1);
        client.field_1755 = new class_437();
        check(!bridge.clearJump(jump) && bridge.capture(2) == null, "menu blocks read/write");
        client.field_1755 = null;
        p.alive = false;
        check(bridge.capture(1) == null && bridge.capture(2) == null, "dead player");
        p.alive = true;
        class_638 world = client.field_1687;
        client.field_1687 = null;
        check(bridge.capture(1) == null, "no world");
        client.field_1687 = world;
        jump = bridge.capture(1);
        Object[] heldJump = jump;
        Throwable[] failure = {null};
        Thread wrongThread = new Thread(() -> {
            try {
                check(bridge.capture(1) == null && !bridge.clearJump(heldJump), "wrong thread");
            } catch (Throwable error) { failure[0] = error; }
        });
        wrongThread.start(); wrongThread.join();
        if (failure[0] != null) throw new AssertionError(failure[0]);
        check(bridge.clearJump(jump), "wrong thread cannot consume client-thread work");
        p.field_6228 = 4;

        GameBridge other = bridge();
        jump = bridge.capture(1);
        check(!other.clearJump(jump), "bridge ownership");
        jump = bridge.capture(1);
        bridge.stop();
        check(bridge.capture(1) == null && !bridge.clearJump(jump), "shutdown rejects retained work");
        GameBridge fresh = bridge();
        check(fresh.clearJump(fresh.capture(1)), "new bridge after shutdown");
        expectBindingFailure(HybridBridgeTest.class.getClassLoader(), 12101);
        expectBindingFailure(new ClassLoader(HybridBridgeTest.class.getClassLoader()) {
            @Override protected Class<?> loadClass(String name, boolean resolve) throws ClassNotFoundException {
                if (name.equals("net.minecraft.class_746")) throw new ClassNotFoundException(name);
                return super.loadClass(name, resolve);
            }
        }, 12111);
        System.out.println("PASS hybrid bridge: " + checks + " checks (" + args[0] + ")");
    }
}

class class_310 extends class_1255 {
    static class_310 field_1700;
    class_746 field_1724 = new class_746();
    class_638 field_1687 = new class_638();
    class_437 field_1755;
    class_315 field_1690 = new class_315();
    int field_1752;
    class_636 field_1761 = new class_636();
    class_239 field_1765;
    final class_1041 window = new class_1041();
    public class_1041 method_22683() { return window; }
    int useCount;
    int attackCount;
    private boolean method_1536() { ++attackCount; return true; }
    boolean failUse;
    private void method_1583() {
        ++useCount; field_1752 = 4;
        if (failUse) { failUse = false; throw new IllegalStateException("Fixture failure after item use"); }
    }
}
class class_1041 {
    public long method_4490() { return 1L; }
}
class class_1255 {
    private final Thread owner = Thread.currentThread();
    public boolean method_18854() { return Thread.currentThread() == owner; }
}
class class_638 {
    final java.util.List<Object> entities = new java.util.ArrayList<>();
    public Iterable<Object> method_18112() { return entities; }
    class_2680 state = new class_2680();
    public class_2680 method_8320(class_2338 position) { return state; }
}
interface class_364 { default boolean method_25370() { return false; } }
interface class_4069 extends class_364 { java.util.List<class_364> method_25396(); }
class class_437 implements class_4069 {
    final java.util.List<class_364> children = new java.util.ArrayList<>();
    public java.util.List<class_364> method_25396() { return children; }
}
class class_342 implements class_364 {
    boolean focused = true, visible = true, active = true;
    public boolean method_25370() { return focused; }
    public boolean method_1885() { return visible; }
    public boolean method_20315() { return active; }
}
class class_507 {
    class_342 field_3089;
    boolean open;
    public boolean method_2605() { return open; }
}
class class_10260 extends class_465 { class_507 field_54474 = new class_507(); }
class class_746 extends class_1657 {
    class_744 field_3913 = new class_744();
    class_10185 field_54156=class_10185.field_54098;
    int field_3935;
    boolean lastSentSprint;
    private void method_46742() {
        if(lastSentSprint!=sprinting) {field_3944.events.add("sprint:"+sprinting);lastSentSprint=sprinting;}
    }
    class_1656 abilities = new class_1656();
    public class_1656 method_31549() { return abilities; }
    boolean eligible = true;
    private boolean method_48300() { return eligible; }
    class_1661 inventory = new class_1661();
    public class_1661 method_31548() { return inventory; }
    class_634 field_3944 = new class_634();
}
class class_634 {
    public void method_2883(Object packet) {}
    java.util.List<String> events=new java.util.ArrayList<>();
    int inputPackets;boolean failAfterSend;
    public void method_52787(class_2596 packet) {
        if(packet instanceof class_2851 input) {++inputPackets;events.add("input:"+input.input.forward);}
        if(failAfterSend) throw new IllegalStateException("after input send");
    }
}
class class_2596 {}
class class_2851 extends class_2596 {
    final class_10185 input;
    public class_2851(class_10185 input) {this.input=input;}
}
class class_2848 extends class_2596 {
    public class_2848(class_1297 entity, class_2848$class_2849 mode) {}
    public class_2848(class_1297 entity, class_2848$class_2849 mode, int data) {}
}
enum class_2848$class_2849 {
    PRESS_SHIFT_KEY, RELEASE_SHIFT_KEY, STOP_SLEEPING, START_SPRINTING, STOP_SPRINTING, START_RIDING_JUMP, STOP_RIDING_JUMP, OPEN_INVENTORY, START_FALL_FLYING;
    public static final class_2848$class_2849 field_12985 = STOP_SPRINTING;
}
class class_1657 extends HybridLivingEntity {
    class_1703 field_7512;
    class_1723 field_7498 = new class_1723();
}
class class_1297 {
    boolean glowing, glowingFlag, failGlow, sprintingFlag;
    public boolean method_36361() { return glowing; }
    protected boolean method_5795(int index) { return (index == 6 && glowingFlag) || (index == 3 && sprintingFlag); }
    public void method_5834(boolean value) {
        glowing = value;
        if (failGlow) { failGlow = false; throw new IllegalStateException("Fixture after glow mutation"); }
    }
    protected void method_5729(int index, boolean value) {
        if (index == 6) glowingFlag = value;
        if (index == 3) sprintingFlag = value;
    }
}
class HybridLivingEntity extends class_1297 {
    int field_6228;
    int field_6235;
    float field_6031, field_5965;
    boolean alive = true, ground = true, water, lava, climbing, gliding, riding;
    class_243 motion = new class_243(0, 0, 0);
    class_238 bounds = new class_238();
    boolean sprinting, usingItem, failSprintWrite;
    class_1799 mainHand = new class_1799(null), offHand = new class_1799(null);
    public boolean method_5805() { return alive; }
    public boolean method_24828() { return ground; }
    public boolean method_5799() { return water; }
    public boolean method_5771() { return lava; }
    public boolean method_6101() { return climbing; }
    public boolean method_6128() { return gliding; }
    public boolean method_5765() { return riding; }
    public class_243 method_18798() { return motion; }
    public class_238 method_5829() { return bounds; }
    public void method_18800(double x, double y, double z) { motion = new class_243(x, y, z); }
    public boolean method_5624() { return sprinting; }
    public boolean method_6115() { return usingItem; }
    public void method_5728(boolean value) {
        sprinting = value;
        if (failSprintWrite) { failSprintWrite = false; throw new IllegalStateException("Fixture mutator failed after write"); }
    }
    public class_1799 method_6047() { return mainHand; }
    public class_1799 method_6079() { return offHand; }
}
class class_744 {
    class_10185 field_54155 = new class_10185();
    class_241 axes = new class_241();
    class_241 field_55868 = axes;
    public class_241 method_3128() { return axes; }
    float field_3905;
    float field_3907;
    boolean field_3904;
}
class class_10185 {
    static final class_10185 field_54098 = new class_10185();
    boolean forward;
    boolean jumping;
    public boolean comp_3163() { return jumping; }
    @Override public boolean equals(Object other) {return other instanceof class_10185 x && x.forward==forward && x.jumping==jumping;}
    @Override public int hashCode() {return (forward?1:0)+(jumping?2:0);}
}
class class_241 { static final class_241 field_1340=new class_241();float field_1343, field_1342; }
class class_243 {
    final double field_1352, field_1351, field_1350;
    class_243(double x, double y, double z) { field_1352 = x; field_1351 = y; field_1350 = z; }
}
class class_1656 { boolean field_7479; }
class class_304 {
    boolean pressed, failWrite;
    class_3675$class_306 field_1655 = new class_3675$class_306(340, class_3675$class_307.field_1668);
    public boolean method_1434() { return pressed; }
    public void method_23481(boolean value) {
        pressed = value;
        if (failWrite) { failWrite = false; throw new IllegalStateException("Fixture after sneak mutation"); }
    }
}
class class_3675$class_307 {
    static final class_3675$class_307 field_1668 = new class_3675$class_307(), field_1671 = new class_3675$class_307(), field_1672 = new class_3675$class_307();
}
class class_3675$class_306 {
    int code; class_3675$class_307 category;
    class_3675$class_306(int code, class_3675$class_307 category) { this.code = code; this.category = category; }
    public int method_1444() { return code; }
    public class_3675$class_307 method_1442() { return category; }
}
class class_238 {
    double field_1323 = 0, field_1322 = 64, field_1321 = 0, field_1320 = .6, field_1324 = .6;
}
class class_315 {
    class_304 field_1894 = new class_304(), field_1881 = new class_304(), field_1913 = new class_304();
    class_304 field_1849 = new class_304(), field_1903 = new class_304();
    class_304 field_1904 = new class_304(), field_1867 = new class_304();
    class_304 field_1832 = new class_304();
    class_7172 gamma = new class_7172();
    public class_7172 method_42473() { return gamma; }
}
class class_7172 { private Object field_37868 = Double.valueOf(.5); }
class class_1792 {
    String translation = "item.minecraft.fixture";
    class_1792() {}
    class_1792(String key) { translation = key; }
    public String method_7876() { return translation; }
}
class class_1747 extends class_1792 {}
class class_1812 extends class_1792 {}
class FixtureSplashPotion extends class_1812 {}
class class_1799 implements class_9473 {
    class_1792 heldItem;
    float miningSpeed = 1;
    boolean suitable;
    int components, stackCount = 1;
    int maximum = 64;
    class_1844 potion;
    class_1799(class_1792 item) { heldItem = item; }
    public class_1792 method_7909() { return heldItem; }
    public boolean method_7960() { return heldItem == null; }
    public int method_7947() { return heldItem == null ? 0 : stackCount; }
    public class_1799 method_7972() {
        class_1799 result = new class_1799(heldItem);
        result.components = components; result.stackCount = stackCount;
        result.maximum = maximum; result.potion = potion;
        return result;
    }
    public static boolean method_31577(class_1799 a, class_1799 b) {
        return a.heldItem == b.heldItem && a.components == b.components && a.potion == b.potion;
    }
    public int method_7914() { return maximum; }
    public Object method_58694(class_9331 type) { return type == class_9334.field_49651 ? potion : null; }
    public float method_7924(class_2680 state) { return miningSpeed; }
    public boolean method_7951(class_2680 state) { return suitable; }
}
class class_1802 { static final class_1792 field_8287 = new class_1792(), field_8634 = new class_1792(); }
class class_2371 extends java.util.ArrayList<Object> {}
interface class_1263 {}
class class_1661 implements class_1263 {
    private int field_7545;
    private class_2371 field_7547 = new class_2371();
    class_1661() { for (int i = 0; i < 9; ++i) field_7547.add(new class_1799(null)); }
}
class class_636 {
    int syncCount;
    int inventoryClicks;
    boolean failInventory;
    private void method_2906(int syncId, int slot, int button, class_1713 type, class_1657 player) {
        if ((type != class_1713.field_7790 && type != class_1713.field_7794 && type != class_1713.field_7791) ||
            syncId != player.field_7512.field_7763 || button < 0 ||
            (type == class_1713.field_7790 ? button > 1 : type == class_1713.field_7791 ? button > 8 : button != 0))
            throw new AssertionError("Wrong inventory transaction");
        ++inventoryClicks;
        if (player.field_7512.simulate) {
            class_1703 h = player.field_7512;
            class_1735 s = (class_1735) h.field_7761.get(slot);
            if (type == class_1713.field_7791) {
                class_1735 target = null;
                for (Object object : h.field_7761) {
                    class_1735 candidate = (class_1735) object;
                    if (candidate.field_7871 == ((class_746) player).inventory && candidate.field_7875 == button) target = candidate;
                }
                if (target == null) throw new AssertionError("Missing hotbar target");
                class_1799 old = target.stack; target.stack = s.stack; s.stack = old;
            }
            else if (type == class_1713.field_7794) s.stack = new class_1799(null);
            else if (h.detailed) {
                if (h.carried.method_7960()) { h.carried = s.stack; s.stack = new class_1799(null); }
                else if (s.stack.method_7960() && s.insertable) {
                    int take = button == 1 ? 1 : h.carried.stackCount;
                    s.stack = h.carried.method_7972(); s.stack.stackCount = take;
                    h.carried.stackCount -= take;
                    if (h.carried.stackCount == 0) h.carried = new class_1799(null);
                } else if (class_1799.method_31577(s.stack, h.carried)) {
                    if (s.insertable) {
                        int take = Math.min(Math.min(s.slotLimit, h.carried.maximum) - s.stack.stackCount,
                            button == 1 ? 1 : h.carried.stackCount);
                        s.stack.stackCount += take; h.carried.stackCount -= take;
                        if (h.carried.stackCount == 0) h.carried = new class_1799(null);
                    } else {
                        h.carried.stackCount += s.stack.stackCount; s.stack = new class_1799(null);
                    }
                }
            }
            else { class_1799 old = s.stack; s.stack = h.carried; h.carried = old; }
        }
        if (failInventory) { failInventory = false; throw new IllegalStateException("Fixture after inventory mutation"); }
    }
    private void method_2911() { ++syncCount; }
}
class class_1713 { static final class_1713 field_7790 = new class_1713(), field_7794 = new class_1713(), field_7791 = new class_1713(); }
class class_1703 {
    int field_7763 = 7;
    class_2371 field_7761 = new class_2371();
    class_1799 carried = new class_1799(null);
    boolean simulate;
    boolean detailed;
    public class_1799 method_34255() { return carried; }
}
class class_1723 extends class_1703 {}
class class_1735 {
    int field_7874;
    class_1799 stack = new class_1799(null);
    class_1263 field_7871;
    int field_7875 = -1, slotLimit = 64;
    boolean enabled = true, insertable = true;
    class_1735(int id) { field_7874 = id; }
    public class_1799 method_7677() { return stack; }
    public boolean method_7682() { return enabled; }
    public boolean method_7680(class_1799 value) { return insertable; }
    public int method_7676(class_1799 value) { return slotLimit; }
}
class class_465 extends class_437 {
    class_1703 field_2797 = new class_1703();
    class_1735 field_2787;
}
class class_490 extends class_10260 {}
class class_476 extends class_465 {}
class class_481 extends class_465 { class_342 field_2894; }
class class_9331 {}
class class_9334 { static final class_9331 field_49651 = new class_9331(); }
interface class_9473 { Object method_58694(class_9331 type); }
class class_6880 {}
class class_1847 { static final class_6880 field_8963 = new class_6880(), field_8980 = new class_6880(); }
class class_1844 {
    final class_6880 type;
    class_1844(class_6880 type) { this.type = type; }
    public boolean method_57401(class_6880 value) { return type == value; }
}
class class_2680 { boolean air; public boolean method_26215() { return air; } }
class class_2338 {
    int x;
    class_2338(int x) { this.x = x; }
    class_2338(int x, int y, int z) { this.x = x; }
    @Override public boolean equals(Object other) { return other instanceof class_2338 pos && x == pos.x; }
    @Override public int hashCode() { return x; }
}
class class_239$class_240 {
    static final class_239$class_240 field_1332 = new class_239$class_240();
    static final class_239$class_240 MISS = new class_239$class_240();
}
class class_239 {
    public class_239$class_240 method_17783() { return class_239$class_240.MISS; }
}
class class_3965 extends class_239 {
    class_2338 pos = new class_2338(1);
    boolean miss;
    public class_2338 method_17777() { return pos; }
    @Override public class_239$class_240 method_17783() {
        return miss ? class_239$class_240.MISS : class_239$class_240.field_1332;
    }
}
