import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.screens.Screen;
import net.minecraft.client.multiplayer.ClientLevel;
import net.onestep.hybrid.UtilityBridge;

public final class NamedSprintTest {
    private static void check(boolean condition, String message) {
        if (!condition) throw new AssertionError(message);
    }
    public static void main(String[] args) throws Exception {
        for (int target : new int[]{26100, 26101, 26102}) {
            Minecraft client = new Minecraft();
            UtilityBridge bridge = new UtilityBridge(NamedSprintTest.class.getClassLoader(), target);
            client.player.eligible = false; net.onestep.hybrid.ManagerTestDriver.utility(bridge,2, true, 1, false);
            check(!client.player.sprinting, "eligibility");
            client.player.eligible = true; client.player.using = true; net.onestep.hybrid.ManagerTestDriver.utility(bridge,2, true, 1, false);
            check(!client.player.sprinting, "using item");
            net.onestep.hybrid.ManagerTestDriver.utility(bridge,2, true, 0, false); check(client.player.sprinting, "start");
            bridge.release(2); check(!client.player.sprinting, "release");
            net.onestep.hybrid.ManagerTestDriver.utility(bridge,2, true, 0, false); client.options.keySprint.down = true; bridge.release(2);
            check(client.player.sprinting, "manual key ownership");
            client.options.keySprint.down = false; client.player.sprinting = false;
            net.onestep.hybrid.ManagerTestDriver.utility(bridge,2, true, 0, false); client.level = new ClientLevel(); bridge.release(2);
            check(client.player.sprinting, "old world untouched");
            client.player.sprinting = false; net.onestep.hybrid.ManagerTestDriver.utility(bridge,2, true, 0, false);
            client.screen = new Screen(); net.onestep.hybrid.ManagerTestDriver.utility(bridge,2, true, 0, false);
            check(!client.player.sprinting, "screen cleanup");
            try { bridge.prepare(3); throw new AssertionError("other features enabled"); }
            catch (IllegalStateException expected) {}
            bridge.stop();
        }
        System.out.println("PASS named Sprint bindings (" + args[0] + ")");
    }
}
