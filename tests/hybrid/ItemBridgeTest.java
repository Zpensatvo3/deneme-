package net.minecraft;
import net.onestep.hybrid.ItemBridge;

public final class ItemBridgeTest {
    private static void check(boolean value, String message) { if (!value) throw new AssertionError(message); }
    public static void main(String[] args) throws Exception {
        class_310 c = new class_310(); class_310.field_1700 = c;
        class_746 p = c.field_1724;
        p.mainHand = new class_1799(class_1802.field_8287);
        ItemBridge b = new ItemBridge(ItemBridgeTest.class.getClassLoader(), 12111);
        Object[] packet = b.captureExp();
        check(packet != null && !b.useExp(packet, false), "physical use required");
        packet = b.captureExp();
        check(b.useExp(packet, true) && c.useCount == 1, "single use request");
        check(!b.useExp(packet, true), "no replay");
        packet = b.captureExp();
        p.mainHand = new class_1799(new class_1792());
        check(!b.useExp(packet, true), "changed hand invalidates request");
        p.mainHand = new class_1799(class_1802.field_8287);
        packet = b.captureExp(); c.field_1687 = new class_638();
        check(!b.useExp(packet, true), "world identity checked");
        packet = b.captureExp(); c.field_1755 = new class_437();
        check(!b.useExp(packet, true) && b.captureExp() == null, "menu blocks use");
        c.field_1755 = null;
        final Object[] pending = b.captureExp();
        final Throwable[] error = {null};
        Thread t = new Thread(() -> { try { check(b.captureExp() == null && !b.useExp(pending, true), "wrong thread"); } catch (Throwable e) { error[0] = e; } });
        t.start(); t.join(); if (error[0] != null) throw new AssertionError(error[0]);
        check(b.useExp(pending, true), "wrong thread cannot consume request");
        packet = b.captureExp(); b.stop();
        check(!b.useExp(packet, true) && b.captureExp() == null, "shutdown");
        System.out.println("PASS Fast EXP bridge lifecycle and ownership (" + args[0] + ")");
    }
}
