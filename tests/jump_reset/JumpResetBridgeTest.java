package net.minecraft;

import java.lang.reflect.Field;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.*;
import java.util.regex.Pattern;
import net.onestep.hybrid.WorldBridge;
import org.lwjgl.glfw.GLFW;

/** Standalone game/platform fixtures. Never sends native input or loads Minecraft. */
public final class JumpResetBridgeTest {
    private static int checks;
    private static void check(boolean value, String message) { ++checks; if (!value) throw new AssertionError(message); }
    private static Field field(String name) throws Exception { Field f = WorldBridge.class.getDeclaredField(name); f.setAccessible(true); return f; }
    private static void time(WorldBridge b, String name, long value) throws Exception { field(name).setLong(b, value); }
    private static boolean flag(WorldBridge b, String name) throws Exception { return field(name).getBoolean(b); }
    private static final class PlatformBridge extends WorldBridge {
        boolean allowed = true; int permissionChecks, revokeAt = -1;
        PlatformBridge() throws Exception { super(JumpResetBridgeTest.class.getClassLoader(), 12111); }
        PlatformBridge(int target, String[] names) throws Exception { super(JumpResetBridgeTest.class.getClassLoader(), target, names); }
        @Override protected boolean jumpPermitted(long generation) {
            return allowed && ++permissionChecks != revokeAt;
        }
    }
    private static class_310 setup() {
        GLFW.reset(); class_310 c = new class_310(); class_310.field_1700 = c; return c;
    }
    private static PlatformBridge bridge() throws Exception { PlatformBridge b = new PlatformBridge(); tick(b); return b; }
    private static void tick(PlatformBridge b) throws Exception { net.onestep.hybrid.ManagerTestDriver.jump(b,true, 1, 0, 100, 0, true); }
    private static void tick(PlatformBridge b, int mode) throws Exception { net.onestep.hybrid.ManagerTestDriver.jump(b,true, 1, mode, 100, 0, true); }
    private static void hit(class_310 c) { c.field_1724.field_6235 = 10; c.field_1724.source = new class_1282(new class_1309()); }
    private static String[] bindings() throws Exception {
        String cpp = Files.readString(Path.of("src/services/game/world_bridge.cpp"));
        var matches = Pattern.compile("\\{\"((?:net/minecraft/class_|field_|method_)[^\"]+)\", ").matcher(cpp);
        Set<String> names = new TreeSet<>(); while (matches.find()) names.add(matches.group(1));
        List<String> pairs = new ArrayList<>(); for (String name : names) { pairs.add(name); pairs.add(name); }
        return pairs.toArray(String[]::new);
    }
    public static void main(String[] args) throws Exception {
        class_310 c = setup(); PlatformBridge b = bridge();
        hit(c); tick(b); check(c.field_1690.field_1903.pressed, "fresh living hit presses configured game jump binding");
        tick(b); check(c.field_1690.field_1903.pressed, "holds until game advances");
        ++c.field_1724.field_6012; tick(b); check(!c.field_1690.field_1903.pressed, "releases after one game tick");
        time(b, "jumpNextAllowed", 0); tick(b); check(!c.field_1690.field_1903.pressed, "same hit never replays");
        c.field_1724.field_6235 = 9; tick(b); hit(c); tick(b); check(c.field_1690.field_1903.pressed, "next rising hurt edge can activate");

        c = setup(); hit(c); b = bridge(); check(!c.field_1690.field_1903.pressed, "enable mid-hit only seeds history");
        c = setup(); b = bridge(); hit(c); net.onestep.hybrid.ManagerTestDriver.jump(b,true, 2, 0, 100, 0, true);
        check(!c.field_1690.field_1903.pressed, "generation changes cannot replay an old hit");
        c = setup(); b = bridge(); net.onestep.hybrid.ManagerTestDriver.jump(b,true, 1, 0, 0, 0, true); hit(c); net.onestep.hybrid.ManagerTestDriver.jump(b,true, 1, 0, 0, 0, true);
        check(!c.field_1690.field_1903.pressed, "zero chance disables activation");
        c = setup(); b = bridge(); net.onestep.hybrid.ManagerTestDriver.jump(b,true, 1, 0, 100, 100, true); hit(c); net.onestep.hybrid.ManagerTestDriver.jump(b,true, 1, 0, 100, 100, true);
        time(b, "jumpReadyAt", Long.MAX_VALUE); net.onestep.hybrid.ManagerTestDriver.jump(b,true, 1, 0, 100, 100, true);
        check(!c.field_1690.field_1903.pressed && flag(b, "jumpPending"), "delay holds pending decision");
        time(b, "jumpReadyAt", 0); net.onestep.hybrid.ManagerTestDriver.jump(b,true, 1, 0, 100, 100, true);
        check(c.field_1690.field_1903.pressed, "configured delay can complete");

        c = setup(); b = bridge(); c.field_1724.field_6235 = 10; tick(b);
        check(!c.field_1690.field_1903.pressed && flag(b, "jumpPending"), "combat-only waits for fresh living source");
        c.field_1724.source = new class_1282(new class_1309()); tick(b);
        check(c.field_1690.field_1903.pressed, "source arriving after hurt can confirm pending hit");
        c = setup(); b = bridge(); c.field_1724.field_6235 = 10; tick(b); time(b, "jumpConfirmUntil", 0);
        c.field_1724.source = new class_1282(new class_1309()); tick(b);
        check(!c.field_1690.field_1903.pressed && !flag(b, "jumpPending"), "late source cannot confirm expired hit");
        c = setup(); c.field_1724.source = new class_1282(new class_1309()); b = bridge(); c.field_1724.field_6235 = 10; tick(b);
        check(!c.field_1690.field_1903.pressed, "preexisting damage source cannot confirm a new hit");
        c = setup(); b = bridge(); c.field_1724.field_6235 = 10; c.field_1724.source = new class_1282(c.field_1724); tick(b);
        check(!c.field_1690.field_1903.pressed, "self damage is not a living attack");
        c = setup(); b = bridge(); c.field_1724.field_6235 = 10; c.field_1724.source = new class_1282(new class_1297()); tick(b);
        check(!c.field_1690.field_1903.pressed, "nonliving source does not pass living-only filter");
        c = setup(); b = bridge(); net.onestep.hybrid.ManagerTestDriver.jump(b,true, 1, 0, 100, 0, false); c.field_1724.field_6235 = 10;
        net.onestep.hybrid.ManagerTestDriver.jump(b,true, 1, 0, 100, 0, false); check(c.field_1690.field_1903.pressed, "combat-only off permits environmental damage");
        c = setup(); b = bridge(); hit(c); c.field_1724.grounded = false; tick(b);
        check(!c.field_1690.field_1903.pressed, "ground grace never authorizes an airborne press");
        time(b, "jumpExpiresAt", 0); c.field_1724.grounded = true; tick(b);
        check(!c.field_1690.field_1903.pressed, "expired decision cannot jump after landing");
        c = setup(); b = bridge(); hit(c); c.field_1724.field_6228 = 2; tick(b);
        check(!c.field_1690.field_1903.pressed, "respects vanilla jump cooldown");
        c.field_1724.field_6228 = 0; tick(b); check(c.field_1690.field_1903.pressed, "ready cooldown allows pending jump");

        for (int reason = 0; reason < 10; ++reason) {
            c = setup(); b = bridge(); hit(c);
            switch (reason) {
                case 0: c.field_1724.using = true; break;
                case 1: c.field_1724.sleeping = true; break;
                case 2: c.field_1724.water = true; break;
                case 3: c.field_1724.swimming = true; break;
                case 4: c.field_1724.fallFlying = true; break;
                case 5: c.field_1724.climbing = true; break;
                case 6: c.field_1724.riding = true; break;
                case 7: c.field_1724.abilities.field_7478 = true; break;
                case 8: c.field_1724.abilities.field_7479 = true; break;
                case 9: c.field_1755 = new class_437(); break;
            }
            tick(b); check(!c.field_1690.field_1903.pressed, "ineligible game context " + reason);
        }
        c = setup(); b = bridge(); GLFW.keys[32] = 1; hit(c); tick(b);
        check(!c.field_1690.field_1903.pressed, "real jump is never adopted");
        c = setup(); b = bridge(); hit(c); tick(b); GLFW.keys[32] = 1; b.cancelJump();
        check(c.field_1690.field_1903.pressed, "cleanup preserves physical hold");
        c = setup(); b = bridge(); hit(c); tick(b); net.onestep.hybrid.ManagerTestDriver.jump(b,false, 1, 0, 100, 0, true);
        check(!c.field_1690.field_1903.pressed, "disable releases owned binding");
        c = setup(); b = bridge(); hit(c); tick(b); b.allowed = false; tick(b);
        check(!c.field_1690.field_1903.pressed, "authorization/focus cancellation releases input");
        c = setup(); b = bridge(); hit(c); tick(b); c.field_1687 = new class_638(); tick(b);
        check(!c.field_1690.field_1903.pressed, "world change releases global options binding");
        c = setup(); b = bridge(); hit(c); tick(b); class_304 old = c.field_1690.field_1903;
        c.field_1690.field_1903 = new class_304(); tick(b);
        check(!c.field_1690.field_1903.pressed && !old.pressed, "old synthetic press is cleared without touching replacement binding");
        c = setup(); b = bridge(); hit(c); tick(b); c.field_1690.field_1903.field_1655 = new class_3675$class_306(70, class_3675$class_307.field_1668);
        tick(b); check(!c.field_1690.field_1903.pressed, "rebinding during owned press cannot leave jump stuck");
        c = setup(); b = bridge(); hit(c); tick(b);
        c.field_1690.field_1903.field_1655 = new class_3675$class_306(70, class_3675$class_307.field_1668);
        GLFW.keys[70]=1; tick(b);
        check(c.field_1690.field_1903.pressed, "rebinding cleanup preserves newly assigned physical key");
        c = setup(); b = bridge(); hit(c); tick(b); time(b, "jumpHoldUntil", 0); tick(b);
        check(!c.field_1690.field_1903.pressed, "stalled game releases hold on timeout");

        c = setup(); b = new PlatformBridge(); tick(b, 1); hit(c); tick(b, 1);
        check(c.field_1690.field_1903.pressed && GLFW.keys[32] == 0, "Game Space presses only the game binding, not GLFW/OS physical state");
        ++c.field_1724.field_6012; tick(b, 1);
        check(!c.field_1690.field_1903.pressed && c.field_1690.field_1903.pressCount == 1, "Game Space releases after a game tick without native receipts");
        c = setup(); c.field_1690.field_1903.field_1655 = new class_3675$class_306(70, class_3675$class_307.field_1668);
        b = new PlatformBridge(); tick(b, 1); hit(c); tick(b, 1);
        check(!c.field_1690.field_1903.pressed, "Game Space requires a Space mapping");
        c = setup(); c.field_1690.field_1903.field_1655 = new class_3675$class_306(0x39, class_3675$class_307.field_1671);
        b = new PlatformBridge(); tick(b, 1); hit(c); tick(b, 1);
        check(c.field_1690.field_1903.pressed, "Space scan code is resolved through GLFW");
        c = setup(); GLFW.scans[32] = 81;
        c.field_1690.field_1903.field_1655 = new class_3675$class_306(81, class_3675$class_307.field_1671);
        b = new PlatformBridge(); tick(b, 1); hit(c); tick(b, 1);
        check(c.field_1690.field_1903.pressed, "Space scan code is platform supplied, not hardcoded");
        c = setup(); GLFW.scans[70] = 60; GLFW.keys[70] = 1;
        c.field_1690.field_1903.field_1655 = new class_3675$class_306(60, class_3675$class_307.field_1671);
        b = bridge(); hit(c); tick(b);
        check(!c.field_1690.field_1903.pressed && GLFW.keyReads > 0, "physical scan-code hold is read through its GLFW key");
        c = setup(); c.field_1690.field_1903.field_1655 = new class_3675$class_306(70, class_3675$class_307.field_1668);
        b = bridge(); hit(c); tick(b); GLFW.keys[70] = 1; b.cancelJump();
        check(c.field_1690.field_1903.pressed, "custom key physical hold survives cleanup");
        c = setup(); c.field_1690.field_1903.field_1655 = new class_3675$class_306(7, class_3675$class_307.field_1672);
        b = bridge(); hit(c); tick(b); GLFW.buttons[7] = 1; b.cancelJump();
        check(c.field_1690.field_1903.pressed && GLFW.mouseReads > 0, "mouse jump binding reads GLFW and preserves a manual press");
        c = setup(); b = bridge(); hit(c); tick(b); GLFW.keys[32] = 1; GLFW.focused = false; tick(b);
        check(!c.field_1690.field_1903.pressed, "unfocused GLFW window cannot restore a held key");
        c = setup(); b = bridge(); hit(c); tick(b); c.window.handle = 99; tick(b);
        check(!c.field_1690.field_1903.pressed, "window replacement cancels old input ownership");
        c = setup(); c.window.handle = 99; b = bridge(); hit(c); tick(b);
        check(GLFW.lastWindow == 99 && c.field_1690.field_1903.pressed, "GLFW reads current game window handle");
        c = setup(); b = bridge(); hit(c); b.permissionChecks = 0; b.revokeAt = 2; tick(b);
        check(!c.field_1690.field_1903.pressed, "permission is rechecked immediately before game input");
        c = setup(); c.field_1690.field_1903.field_1655 = new class_3675$class_306(60, class_3675$class_307.field_1671);
        b = bridge(); hit(c); tick(b); check(!c.field_1690.field_1903.pressed, "unresolvable scan code fails closed");
        c = setup(); c.field_1690.field_1903.field_1655 = new class_3675$class_306(2800, class_3675$class_307.field_1668);
        b = bridge(); hit(c); tick(b); check(!c.field_1690.field_1903.pressed && GLFW.keyReads == 0, "unknown key never reaches GLFW");
        c = setup(); b = bridge(); hit(c); c.field_1690.field_1903.failPress = true;
        boolean failed = false; try { tick(b); } catch (ReflectiveOperationException expected) { failed = true; }
        c.field_1690.field_1903.failPress = false; tick(b);
        check(failed && !c.field_1690.field_1903.pressed && c.field_1690.field_1903.pressCount == 1, "partially applied press cleans up and suspends instead of replaying");
        c = setup(); b = bridge(); hit(c); final PlatformBridge threaded = b;
        Thread worker = new Thread(() -> { try { tick(threaded); } catch (Exception e) { throw new RuntimeException(e); } });
        worker.start(); worker.join(); check(!c.field_1690.field_1903.pressed, "worker cannot mutate game input");
        b.stop(); tick(b); check(!c.field_1690.field_1903.pressed, "stop prevents later interaction");
        for (int target : new int[]{12111, 26100, 26101, 26102}) {
            c = setup(); b = new PlatformBridge(target, bindings()); tick(b); hit(c); tick(b);
            check(c.field_1690.field_1903.pressed, "exact supplied binding protocol for " + target);
        }
        boolean unsupported = false;
        try { new PlatformBridge(99999, bindings()); } catch (IllegalArgumentException expected) { unsupported = true; }
        check(unsupported, "unsupported target fails closed");
        System.out.println("PASS Jump Reset Java controller: " + checks + " checks (" + args[0] + "; isolated fixtures)");
    }
}
class class_1297 {
    public int field_6012 = 100; boolean grounded = true, water, swimming, riding;
    public boolean method_5805() { return true; } public boolean method_24828() { return grounded; }
    public boolean method_5799() { return water; } public boolean method_5681() { return swimming; }
    public boolean method_5765() { return riding; }
}
class class_1309 extends class_1297 {
    public int field_6235, field_6228; boolean using, sleeping, fallFlying, climbing;
    class_1282 source;
    public boolean method_6115() { return using; } public boolean method_6113() { return sleeping; }
    public boolean method_6128() { return fallFlying; } public boolean method_6101() { return climbing; }
    public class_1282 method_6081() { return source; }
}
class class_746 extends class_1309 {
    class_1656 abilities = new class_1656(); public class_1656 method_31549() { return abilities; }
}
class class_1656 { public boolean field_7478, field_7479; }
class class_1282 { class_1297 attacker; class_1282(class_1297 e) { attacker = e; } public class_1297 method_5529() { return attacker; } }
class class_310 {
    public static class_310 field_1700; public class_746 field_1724 = new class_746();
    public class_638 field_1687 = new class_638(); public class_315 field_1690 = new class_315();
    public class_437 field_1755; public class_636 field_1761 = new class_636();
    class_1041 window = new class_1041(); public class_1041 method_22683() { return window; }
    private final Thread thread = Thread.currentThread(); public boolean method_18854() { return Thread.currentThread() == thread; }
}
class class_638 {} class class_437 {} class class_636 {}
class class_315 { public class_304 field_1903 = new class_304(); }
class class_304 {
    public class_3675$class_306 field_1655 = new class_3675$class_306(32, class_3675$class_307.field_1668);
    boolean pressed, failPress; int pressCount;
    public boolean method_1434() { return pressed; }
    public void method_23481(boolean value) { pressed = value; if (value) { ++pressCount; if (failPress) throw new IllegalStateException("partial press"); } }
}
class class_3675$class_306 {
    int code; class_3675$class_307 type;
    class_3675$class_306(int code, class_3675$class_307 type) { this.code = code; this.type = type; }
    public int method_1444() { return code; } public class_3675$class_307 method_1442() { return type; }
}
class class_3675$class_307 {
    public static final class_3675$class_307 field_1668 = new class_3675$class_307();
    public static final class_3675$class_307 field_1671 = new class_3675$class_307();
    public static final class_3675$class_307 field_1672 = new class_3675$class_307();
}

class class_1041 { long handle = 17; public long method_4490() { return handle; } }
