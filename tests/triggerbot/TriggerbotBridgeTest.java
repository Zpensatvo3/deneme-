package net.minecraft;

import java.lang.reflect.Field;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.*;
import java.util.regex.Pattern;
import net.onestep.hybrid.WorldBridge;
import org.lwjgl.glfw.GLFW;

/** Runs the production controller; only Minecraft and native coordinator are fixtures. */
public final class TriggerbotBridgeTest {
    private static int checks, cancellations, completed;
    private static long manual;
    private static boolean allowed, revokeBeforeAttack;
    private static void check(boolean value, String message) { ++checks; if (!value) throw new AssertionError(message); }
    public static long service(int operation, Object player, Object world, Object entity, int value, long epoch) {
        switch (operation) {
            case 0: case 1: return allowed ? 1 : 0;
            case 2: case 6: return -1;
            case 4: ++completed; return 1;
            case 5: ++cancellations; return 1;
            case 8: return revokeBeforeAttack ? -1 : manual;
            default: throw new AssertionError("Unexpected coordinator operation " + operation);
        }
    }
    private static Field field(String name) throws Exception {
        Field f = WorldBridge.class.getDeclaredField(name); f.setAccessible(true); return f;
    }
    private static void time(WorldBridge b, String name, long value) throws Exception { field(name).setLong(b, value); }
    private static class_310 setup() {
        GLFW.reset(); manual = 0; allowed = true; revokeBeforeAttack = false; cancellations = completed = 0;
        class_310 c = new class_310(); class_310.field_1700 = c;
        c.target = new class_1657(); c.field_1765 = new class_3966(c.target); return c;
    }
    private static void tick(WorldBridge b) throws Exception { tick(b, 0, 0, false); }
    private static void tick(WorldBridge b, int reaction, int criticals, boolean onClick) throws Exception {
        b.tickTrigger(true, 1, manual, .9f, .9f, reaction, reaction, 0, 0, criticals, onClick, true, false);
    }
    private static void ready(WorldBridge b) throws Exception { time(b, "triggerReady", System.nanoTime() - 1_000_000_000L); }
    private static WorldBridge bridge() throws Exception { return new WorldBridge(TriggerbotBridgeTest.class.getClassLoader(), 12111); }
    private static String[] bindings() throws Exception {
        String cpp = Files.readString(Path.of("src/services/game/world_bridge.cpp"));
        var m = Pattern.compile("\\{\"((?:net/minecraft/class_|field_|method_)[^\"]+)\", ").matcher(cpp);
        Set<String> names = new TreeSet<>(); while (m.find()) names.add(m.group(1));
        List<String> pairs = new ArrayList<>(); for (String name : names) { pairs.add(name); pairs.add(name); }
        return pairs.toArray(String[]::new);
    }
    public static void main(String[] args) throws Exception {
        System.load(args[1]);
        class_310 c = setup(); WorldBridge b = bridge(); tick(b);
        check(c.field_1761.attacks == 1 && c.field_1724.swings == 1 && completed == 1, "one vanilla attack and swing");
        time(b, "triggerNotBefore", System.nanoTime() + 60_000_000_000L);
        tick(b); check(c.field_1761.attacks == 1, "minimum interval prevents burst");
        time(b, "triggerNotBefore", 0); c.field_1724.cooldown = .2f; tick(b);
        check(c.field_1761.attacks == 1, "consumed cooldown prevents replay");

        c = setup(); b = bridge(); tick(b, 150, 0, false); time(b, "triggerReady", System.nanoTime() + 60_000_000_000L);
        tick(b, 150, 0, false); check(c.field_1761.attacks == 0, "reaction delay blocks early attack");
        ready(b); tick(b, 150, 0, false); check(c.field_1761.attacks == 1, "elapsed reaction permits attack");
        c = setup(); b = bridge(); tick(b, 150, 0, false); ready(b);
        c.target = new class_1657(); c.field_1765 = new class_3966(c.target); tick(b, 150, 0, false);
        check(c.field_1761.attacks == 0, "new target restarts reaction");
        c = setup(); b = bridge(); tick(b, 150, 0, false); ready(b); ++manual; tick(b, 150, 0, false);
        check(c.field_1761.attacks == 0, "manual click restarts reaction");
        c = setup(); b = bridge(); tick(b, 150, 0, false); ready(b); c.field_1687 = new class_638(); tick(b, 150, 0, false);
        check(c.field_1761.attacks == 0, "world replacement restarts reaction");
        c = setup(); b = bridge(); tick(b, 150, 0, false); ready(b);
        b.tickTrigger(true, 2, 0, .9f, .9f, 150, 150, 0, 0, 0, false, true, false);
        check(c.field_1761.attacks == 0, "generation change restarts reaction");

        for (int reason = 0; reason < 9; ++reason) {
            c = setup(); b = bridge();
            switch (reason) {
                case 0: c.field_1755 = new class_437(); break;
                case 1: c.target.alive = false; break;
                case 2: c.target.invisible = true; break;
                case 3: c.field_1724.using = true; break;
                case 4: allowed = false; break;
                case 5: revokeBeforeAttack = true; break;
                case 6: c.fresh = false; break;
                case 7: c.field_1724.inventory.field_7545 = -1; break;
                case 8: c.field_1724.cooldown = Float.NaN; break;
            }
            tick(b); check(c.field_1761.attacks == 0, "invalid context cancels " + reason);
        }
        c = setup(); b = bridge(); tick(b, 0, 0, true);
        check(c.field_1761.attacks == 0, "on-click requires physical hold");
        GLFW.buttons[0] = 1; tick(b, 0, 0, true);
        check(c.field_1761.attacks == 1, "physical hold permits on-click");
        c = setup(); b = bridge(); tick(b, 0, 2, false);
        check(c.field_1761.attacks == 0, "critical-only rejects grounded hit");
        c.field_1724.grounded = false; c.field_1724.field_6017 = .1; tick(b, 0, 2, false);
        check(c.field_1761.attacks == 1, "falling critical permits hit");
        c = setup(); b = bridge(); c.field_1724.grounded = false; tick(b, 0, 1, false);
        check(c.field_1761.attacks == 0, "smart critical waits while rising");
        time(b, "triggerCritical", System.nanoTime() - 200_000_000L); tick(b, 0, 1, false);
        check(c.field_1761.attacks == 1, "smart critical wait expires");

        c = setup(); b = bridge(); tick(b, 150, 0, false);
        b.tickTrigger(false, 1, 0, .9f, .9f, 0, 0, 0, 0, 0, false, true, false);
        check(c.field_1761.attacks == 0 && field("triggerTarget").get(b) == null, "disable clears pending target");
        c = setup(); b = bridge(); c.field_1761.fail = true;
        boolean failed = false; try { tick(b); } catch (ReflectiveOperationException expected) { failed = true; }
        c.field_1761.fail = false; time(b, "triggerNotBefore", 0); tick(b);
        check(failed && c.field_1761.attacks == 1 && field("triggerFailed").getBoolean(b), "partial attack failure suspends without replay");
        c = setup(); b = bridge(); final WorldBridge threaded = b; final Throwable[] error = {null};
        Thread worker = new Thread(() -> { try { tick(threaded); } catch (Throwable e) { error[0] = e; } });
        worker.start(); worker.join(); check(error[0] == null && c.field_1761.attacks == 0, "worker cannot attack");
        b.stop(); tick(b); check(c.field_1761.attacks == 0, "stopped bridge cannot attack");
        for (int target : new int[]{12111, 26100, 26101, 26102}) {
            c = setup(); b = new WorldBridge(TriggerbotBridgeTest.class.getClassLoader(), target, bindings()); tick(b);
            check(c.field_1761.attacks == 1, "supplied binding protocol " + target);
        }
        System.out.println("PASS Triggerbot Java controller: " + checks + " checks (" + args[0] + "; offline game/JNI fixtures)");
    }
}
class class_1297 {
    public double field_6017; public float field_6031; public class_243 field_22467 = new class_243();
    boolean alive = true, invisible, grounded = true;
    public boolean method_5805() { return alive; } public boolean method_5767() { return invisible; }
    public boolean method_24828() { return grounded; } public boolean method_5799() { return false; }
    public boolean method_5681() { return false; } public boolean method_5765() { return false; }
    public boolean method_5624() { return false; } public int method_5628() { return 7; }
}
class class_1309 extends class_1297 {
    public int field_6273 = 100; boolean using;
    public boolean method_6115() { return using; } public boolean method_6128() { return false; }
    public boolean method_6101() { return false; } public boolean method_6059(class_6880 entry) { return false; }
    public boolean method_6039() { return false; }
}
class class_1657 extends class_1309 {}
class class_1308 extends class_1309 {} class class_1429 extends class_1308 {}
class class_746 extends class_1657 {
    class_1661 inventory = new class_1661(); float cooldown = 1; int swings;
    public class_1661 method_31548() { return inventory; }
    public float method_7261(float delta) { return cooldown; }
    public double method_55754() { return 4.5; } public double method_55755() { return 3.; }
    public void method_6104(class_1268 hand) { ++swings; }
    public static class_239 method_76763(class_1297 player, double blocks, double entities, float delta) {
        return class_310.field_1700.fresh ? class_310.field_1700.field_1765 : new class_239();
    }
}
class class_310 {
    public static class_310 field_1700; public class_746 field_1724 = new class_746();
    public class_638 field_1687 = new class_638(); public class_437 field_1755;
    public class_315 field_1690 = new class_315(); public class_636 field_1761 = new class_636();
    public class_239 field_1765; class_1657 target; boolean fresh = true;
    private final Thread thread = Thread.currentThread();
    public boolean method_18854() { return Thread.currentThread() == thread; }
    public class_1297 method_1560() { return field_1724; }
    public class_1041 method_22683() { return new class_1041(); }
}
class class_636 {
    int attacks; boolean fail;
    public void method_2918(class_1657 player, class_1297 entity) {
        ++attacks; if (fail) throw new IllegalStateException("failure after attack");
    }
}
class class_1661 { public int field_7545; public class_1799 method_5438(int slot) { return new class_1799(); } }
class class_1799 { public boolean method_7960() { return false; } public class_1792 method_7909() { return new class_1792(); } }
class class_1792 { public class_6880$class_6883 method_40131() { return new class_6880$class_6883(); } }
class class_1743 extends class_1792 {}
class class_243 { public double field_1352, field_1350; }
class class_239 {} class class_3966 extends class_239 { public class_1297 field_17592; class_3966(class_1297 e) { field_17592 = e; } }
class class_6880 {} class class_6880$class_6883 extends class_6880 { public boolean method_40220(class_6862 tag) { return true; } }
class class_6862 {} class class_3489 { public static class_6862 field_42611 = new class_6862(); }
class class_1294 { public static class_6880 field_5919 = new class_6880(); }
class class_1268 { public static class_1268 field_5808 = new class_1268(); }
class class_1041 { public long method_4490() { return 17; } }
class class_638 {} class class_437 {} class class_315 {}
