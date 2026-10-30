import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.screens.Screen;
import net.minecraft.world.item.BlockItem;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.PotionItem;
import net.minecraft.world.item.SplashPotionItem;
import net.onestep.hybrid.UtilityBridge;

public final class NamedFastPlaceTest {
    private static void check(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }
    public static void main(String[] args) throws Exception {
        for (int target : new int[]{26100, 26101, 26102}) {
            Minecraft client = new Minecraft();
            UtilityBridge bridge = new UtilityBridge(NamedFastPlaceTest.class.getClassLoader(), target);
            client.player.main = new ItemStack(new BlockItem());
            client.setDelay(4); net.onestep.hybrid.ManagerTestDriver.utility(bridge,1, true, 1, false);
            check(client.getDelay() == 4, "requires held input");
            client.options.keyUse.down = true; net.onestep.hybrid.ManagerTestDriver.utility(bridge,1, true, 1, false);
            check(client.getDelay() == 1, "block use");
            net.onestep.hybrid.ManagerTestDriver.utility(bridge,1, true, 4, false); check(client.getDelay() == 1, "never increase delay");
            client.options.keyUse.down = false; client.player.main = new ItemStack(new Item());
            client.player.off = new ItemStack(new PotionItem()); client.setDelay(4);
            net.onestep.hybrid.ManagerTestDriver.utility(bridge,1, true, -1, true); check(client.getDelay() == 0, "physical input and offhand potion");
            client.player.off = new ItemStack(new SplashPotionItem()); client.setDelay(4);
            net.onestep.hybrid.ManagerTestDriver.utility(bridge,1, true, 2, true); check(client.getDelay() == 2, "splash potion subclass");
            client.player.off = new ItemStack(null); client.setDelay(4);
            net.onestep.hybrid.ManagerTestDriver.utility(bridge,1, true, 0, true); check(client.getDelay() == 4, "unsupported item");
            client.player.main = new ItemStack(new BlockItem()); client.screen = new Screen();
            net.onestep.hybrid.ManagerTestDriver.utility(bridge,1, true, 0, true); check(client.getDelay() == 4, "screen blocks operation");
            client.screen = null; client.player.alive = false;
            net.onestep.hybrid.ManagerTestDriver.utility(bridge,1, true, 0, true); check(client.getDelay() == 4, "dead player");
            client.player.alive = true;
            net.onestep.hybrid.ManagerTestDriver.utility(bridge,2, true, 0, false);
            check(client.player.sprinting, "Sprint can share the bridge");
            net.onestep.hybrid.ManagerTestDriver.utility(bridge,1, true, 1, true);
            check(client.getDelay() == 1 && client.player.sprinting, "Fast Place and Sprint coexist");
            bridge.release(2); client.setDelay(4);
            Throwable[] failure = {null};
            Thread worker = new Thread(() -> {
                try { net.onestep.hybrid.ManagerTestDriver.utility(bridge,1, true, 0, true); } catch (Exception e) { failure[0] = e; }
            });
            worker.start(); worker.join();
            check(failure[0] == null && client.getDelay() == 4, "wrong thread");
            bridge.stop(); net.onestep.hybrid.ManagerTestDriver.utility(bridge,1, true, 0, true);
            check(client.getDelay() == 4, "stopped");
        }
        System.out.println("PASS named Fast Place bindings (" + args[0] + ")");
    }
}
