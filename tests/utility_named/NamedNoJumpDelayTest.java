import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.screens.Screen;
import net.minecraft.client.multiplayer.ClientLevel;
import net.onestep.hybrid.GameBridge;

public final class NamedNoJumpDelayTest {
    private static void check(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }
    public static void main(String[] args) throws Exception {
        for (int target : new int[]{26100, 26101, 26102}) {
            Minecraft c = new Minecraft();
            GameBridge bridge = new GameBridge(NamedNoJumpDelayTest.class.getClassLoader(), target);
            c.player.setJumpDelay(5); check(bridge.tick() && c.player.getJumpDelay() == 0, "clear delay");
            check(!bridge.tick(), "zero delay not written");
            c.player.setJumpDelay(-1); check(!bridge.tick() && c.player.getJumpDelay() == -1, "negative delay untouched");
            c.player.setJumpDelay(5); Object[] stale = bridge.capture(1); c.player.setJumpDelay(4);
            check(!bridge.clearJump(stale) && c.player.getJumpDelay() == 4, "changed delay rejected");
            stale = bridge.capture(1); c.level = new ClientLevel();
            check(!bridge.clearJump(stale) && c.player.getJumpDelay() == 4, "old world rejected");
            c.screen = new Screen(); check(!bridge.tick(), "screen blocked"); c.screen = null;
            c.player.alive = false; check(!bridge.tick(), "dead player blocked"); c.player.alive = true;
            Throwable[] failure = {null};
            Thread worker = new Thread(() -> { try { check(!bridge.tick(), "wrong thread"); } catch (Throwable e) { failure[0] = e; } });
            worker.start(); worker.join(); check(failure[0] == null && c.player.getJumpDelay() == 4, "worker did not write");
            bridge.stop(); check(!bridge.tick() && c.player.getJumpDelay() == 4, "shutdown");
        }
        System.out.println("PASS named No Jump Delay bindings (" + args[0] + ")");
    }
}
