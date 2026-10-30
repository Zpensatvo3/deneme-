package net.minecraft;

import java.lang.reflect.Field;
import net.onestep.hybrid.UtilityBridge;

public final class UtilityBridgeTest {
    private static int checks;
    private static void check(boolean condition, String label) {
        ++checks; if (!condition) throw new AssertionError(label);
    }
    private static UtilityBridge bridge() throws Exception {
        return new UtilityBridge(UtilityBridgeTest.class.getClassLoader(), 12111);
    }
    public static void main(String[] args) throws Exception {
        class_310 client = new class_310(); class_310.field_1700 = client;
        class_746 p = client.field_1724;
        UtilityBridge bridge = bridge();
        client.field_1752 = 4;
        p.mainHand = new class_1799(new class_1747());
        net.onestep.hybrid.ManagerTestDriver.utility(bridge,1, true, 1, false);
        check(client.field_1752 == 4, "no held input");
        client.field_1690.field_1904.pressed = true;
        net.onestep.hybrid.ManagerTestDriver.utility(bridge,1, true, 1, false);
        check(client.field_1752 == 1, "game use binding");
        net.onestep.hybrid.ManagerTestDriver.utility(bridge,1, true, 4, false);
        check(client.field_1752 == 1, "never increase cooldown");
        client.field_1690.field_1904.pressed = false;
        client.field_1752 = 4;
        net.onestep.hybrid.ManagerTestDriver.utility(bridge,1, true, -100, true);
        check(client.field_1752 == 0, "physical input and lower clamp");
        client.field_1752 = 6;
        net.onestep.hybrid.ManagerTestDriver.utility(bridge,1, true, 100, true);
        check(client.field_1752 == 4, "upper clamp");
        p.mainHand = new class_1799(new class_1792()); client.field_1752 = 4;
        net.onestep.hybrid.ManagerTestDriver.utility(bridge,1, true, 0, true);
        check(client.field_1752 == 4, "unsupported item");
        p.offHand = new class_1799(new FixtureSplashPotion());
        net.onestep.hybrid.ManagerTestDriver.utility(bridge,1, true, 0, true);
        check(client.field_1752 == 0, "splash potion in offhand");
        p.offHand = new class_1799(new class_1812()); client.field_1752 = 4;
        net.onestep.hybrid.ManagerTestDriver.utility(bridge,1, true, 2, true);
        check(client.field_1752 == 2, "drinkable potion cooldown only");
        client.field_1755 = new class_437(); client.field_1752 = 4;
        net.onestep.hybrid.ManagerTestDriver.utility(bridge,1, true, 0, true);
        check(client.field_1752 == 4, "screen blocks Fast Place");
        client.field_1755 = null; p.alive = false;
        net.onestep.hybrid.ManagerTestDriver.utility(bridge,1, true, 0, true);
        check(client.field_1752 == 4, "dead player"); p.alive = true;

        p.eligible = false; net.onestep.hybrid.ManagerTestDriver.utility(bridge,2, true, 1, false);
        check(!p.sprinting, "vanilla eligibility respected");
        p.eligible = true; p.usingItem = true; net.onestep.hybrid.ManagerTestDriver.utility(bridge,2, true, 1, false);
        check(!p.sprinting, "stop while using setting");
        net.onestep.hybrid.ManagerTestDriver.utility(bridge,2, true, 0, false);
        check(p.sprinting, "using setting disabled");
        net.onestep.hybrid.ManagerTestDriver.utility(bridge,2, false, 0, false);
        check(!p.sprinting, "disable restores owned sprint");
        p.usingItem = false; p.sprinting = true;
        net.onestep.hybrid.ManagerTestDriver.utility(bridge,2, true, 1, false); bridge.release(2);
        check(p.sprinting, "pre-existing manual sprint preserved");
        p.sprinting = false; net.onestep.hybrid.ManagerTestDriver.utility(bridge,2, true, 1, false);
        client.field_1690.field_1867.pressed = true; bridge.release(2);
        check(p.sprinting, "manual sprint key takes over on release");
        client.field_1690.field_1867.pressed = false; p.sprinting = false;
        net.onestep.hybrid.ManagerTestDriver.utility(bridge,2, true, 1, false);
        client.field_1687 = new class_638(); bridge.release(2);
        check(p.sprinting, "never write to previous world owner");
        p.sprinting = false; net.onestep.hybrid.ManagerTestDriver.utility(bridge,2, true, 1, false);
        client.field_1755 = new class_437(); net.onestep.hybrid.ManagerTestDriver.utility(bridge,2, true, 1, false);
        check(!p.sprinting, "menu releases module-owned sprint");
        client.field_1755 = null; p.failSprintWrite = true;
        try { net.onestep.hybrid.ManagerTestDriver.utility(bridge,2, true, 1, false); throw new AssertionError("Expected mutator error"); }
        catch (ReflectiveOperationException expected) { ++checks; }
        check(bridge.release(2) && !p.sprinting, "cleanup after partial mutator failure");

        Field gammaValue = class_7172.class.getDeclaredField("field_37868"); gammaValue.setAccessible(true);
        net.onestep.hybrid.ManagerTestDriver.utility(bridge,3, true, 0, false);
        check(gammaValue.get(client.field_1690.gamma).equals(16.0), "Full Bright");
        net.onestep.hybrid.ManagerTestDriver.utility(bridge,3, false, 0, false);
        check(gammaValue.get(client.field_1690.gamma).equals(.5), "gamma restored on disable");
        net.onestep.hybrid.ManagerTestDriver.utility(bridge,3, true, 0, false); gammaValue.set(client.field_1690.gamma, .8);
        bridge.release(3);
        check(gammaValue.get(client.field_1690.gamma).equals(.8), "manual gamma edit preserved");
        net.onestep.hybrid.ManagerTestDriver.utility(bridge,3, true, 0, false); gammaValue.set(client.field_1690.gamma, .7);
        net.onestep.hybrid.ManagerTestDriver.utility(bridge,3, true, 0, false); bridge.release(3);
        check(gammaValue.get(client.field_1690.gamma).equals(.7), "latest manual preference restored");
        client.field_1687 = null; client.field_1724 = null; client.field_1755 = new class_437();
        net.onestep.hybrid.ManagerTestDriver.utility(bridge,3, true, 0, false);
        check(gammaValue.get(client.field_1690.gamma).equals(16.0), "visual works in main menu without world");
        bridge.release(3);
        class_7172 oldGamma = client.field_1690.gamma;
        net.onestep.hybrid.ManagerTestDriver.utility(bridge,3, true, 0, false); client.field_1690.gamma = new class_7172();
        bridge.release(3);
        check(gammaValue.get(client.field_1690.gamma).equals(.5), "replaced option is not overwritten");
        gammaValue.set(client.field_1690.gamma, Double.NaN); net.onestep.hybrid.ManagerTestDriver.utility(bridge,3, true, 0, false);
        check(Double.isNaN((double) gammaValue.get(client.field_1690.gamma)), "nonfinite gamma rejected");
        gammaValue.set(client.field_1690.gamma, .4);
        client.field_1687 = new class_638(); client.field_1724 = p; client.field_1755 = null;
        net.onestep.hybrid.ManagerTestDriver.utility(bridge,3, true, 0, false);
        Throwable[] error = {null};
        Thread worker = new Thread(() -> {
            try {
                net.onestep.hybrid.ManagerTestDriver.utility(bridge,1, true, 0, true); net.onestep.hybrid.ManagerTestDriver.utility(bridge,2, true, 0, false);
                check(!bridge.release(3), "wrong-thread cleanup deferred");
            } catch (Throwable failure) { error[0] = failure; }
        });
        client.field_1752 = 4; p.sprinting = false;
        worker.start(); worker.join(); if (error[0] != null) throw new AssertionError(error[0]);
        check(client.field_1752 == 4 && !p.sprinting, "worker cannot mutate input/sprint");
        check(bridge.release(3) && gammaValue.get(client.field_1690.gamma).equals(.4), "render cleanup succeeds");
        UtilityBridge isolated = new UtilityBridge(new ClassLoader(UtilityBridgeTest.class.getClassLoader()) {
            @Override protected Class<?> loadClass(String name, boolean resolve) throws ClassNotFoundException {
                if (name.equals("net.minecraft.class_1799")) throw new ClassNotFoundException(name);
                return super.loadClass(name, resolve);
            }
        }, 12111);
        try { isolated.prepare(1); throw new AssertionError("Missing item binding accepted"); }
        catch (ReflectiveOperationException expected) { ++checks; }
        net.onestep.hybrid.ManagerTestDriver.utility(isolated,3, true, 0, false);
        check(gammaValue.get(client.field_1690.gamma).equals(16.0), "missing item binding isolated from visual feature");
        isolated.release(3); isolated.stop();
        bridge.stop(); net.onestep.hybrid.ManagerTestDriver.utility(bridge,3, true, 0, false);
        check(gammaValue.get(client.field_1690.gamma).equals(.4), "stopped bridge never writes");
        check(gammaValue.get(oldGamma).equals(16.0), "stale option intentionally untouched");
        System.out.println("PASS utility bridge: " + checks + " checks (" + args[0] + ")");
    }
}
