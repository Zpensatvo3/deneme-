package net.minecraft;

import java.lang.reflect.Field;
import net.onestep.hybrid.WorldBridge;

public final class WorldBridgeTest {
    private static int checks;
    private static void check(boolean value, String message) { ++checks; if (!value) throw new AssertionError(message); }
    private static WorldBridge bridge() throws Exception {
        // Exercise the exact constructor used by the native connector, including
        // the class-name coverage needed by the retained capabilities.
        String cpp = java.nio.file.Files.readString(java.nio.file.Path.of("src/services/game/world_bridge.cpp"));
        var matches = java.util.regex.Pattern.compile("\\{\"((?:net/minecraft/class_|field_|method_)[^\"]+)\", ").matcher(cpp);
        var names = new java.util.TreeSet<String>(); while (matches.find()) names.add(matches.group(1));
        var pairs = new java.util.ArrayList<String>(); for (String name : names) { pairs.add(name); pairs.add(name); }
        return new WorldBridge(WorldBridgeTest.class.getClassLoader(), 12111, pairs.toArray(String[]::new));
    }
    private static Field field(String name) throws Exception { Field f = class_1661.class.getDeclaredField(name); f.setAccessible(true); return f; }
    private static class_310 setup() throws Exception {
        class_310 c = new class_310(); class_310.field_1700 = c;
        c.field_1724.mainHand = new class_1799(new class_1747());
        ((class_2371) field("field_7547").get(c.field_1724.inventory)).set(3, new class_1799(class_1802.field_8634));
        return c;
    }
    private static int selected(class_310 c) throws Exception { return field("field_7545").getInt(c.field_1724.inventory); }
    public static void main(String[] args) throws Exception {
        class_310 c = setup(); WorldBridge b = bridge(); Object[] p = b.capturePearl();
        check(((double[]) p[6])[7] == 1, "pearl snapshot identifies hotbar slot");
        check(b.beginPearl(p, 3) && selected(c) == 3 && c.field_1761.syncCount == 1, "select and vanilla sync");
        check(!b.beginPearl(p, 3), "single pending pearl transaction");
        check(b.usePearl() && c.useCount == 1 && !b.usePearl(), "one vanilla use without hold/replay");
        check(b.releasePearl() && selected(c) == 0 && c.field_1761.syncCount == 2, "restore owned slot");
        check(b.beginPearl(b.capturePearl(), 3), "new cycle");
        field("field_7545").setInt(c.field_1724.inventory, 6);
        check(b.capturePearl() == null && !b.usePearl(), "manual slot change cancels pending use");
        check(b.releasePearl() && selected(c) == 6, "manual slot is not restored over");
        c = setup(); b = bridge(); p = b.capturePearl();
        class_2371 stacks = (class_2371) field("field_7547").get(c.field_1724.inventory);
        ((class_1799) stacks.get(3)).components++;
        check(!b.beginPearl(p, 3) && selected(c) == 0, "pearl metadata checked before selection");
        check(b.beginPearl(b.capturePearl(), 3), "begin before failure");
        c.failUse = true; boolean failed = false;
        try { b.usePearl(); } catch (ReflectiveOperationException expected) { failed = true; }
        check(failed && c.useCount == 1 && !b.usePearl(), "exception after use cannot cause second throw");
        check(b.releasePearl() && selected(c) == 0, "failed use still releases owned selection");
        c = setup(); b = bridge(); b.beginPearl(b.capturePearl(), 3);
        c.field_1687 = new class_638();
        check(!b.usePearl() && b.releasePearl() && selected(c) == 3, "new world never receives old slot restore");
        p = b.capturePearl(); c.field_1755 = new class_437();
        check(!b.beginPearl(p, 3), "screen blocks new writes");
        c = setup(); b = bridge(); final WorldBridge threaded = b;
        final boolean[] rejected = {false};
        Thread worker = new Thread(() -> { try { rejected[0] = threaded.capturePearl() == null; }
            catch (Exception error) { throw new AssertionError(error); } });
        worker.start(); worker.join(); check(rejected[0], "client thread only");
        b.stop(); check(b.capturePearl() == null, "stop rejects future work");
        System.out.println("PASS world bridge: " + checks + " checks (" + args[0] + ")");
    }
}
