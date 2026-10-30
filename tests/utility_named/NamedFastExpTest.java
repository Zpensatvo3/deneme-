import java.lang.reflect.Field;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.screens.Screen;
import net.minecraft.client.multiplayer.ClientLevel;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.Items;
import net.onestep.hybrid.ItemBridge;

public final class NamedFastExpTest {
    private static void check(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }
    private static void ready(ItemBridge bridge) throws Exception {
        Field field = ItemBridge.class.getDeclaredField("nextUseNanos");
        field.setAccessible(true); field.setLong(bridge, System.nanoTime() - 1);
    }
    public static void main(String[] args) throws Exception {
        for (int target : new int[]{26100, 26101, 26102}) {
            Minecraft c = new Minecraft();
            ItemBridge bridge = new ItemBridge(NamedFastExpTest.class.getClassLoader(), target);
            c.player.main = new ItemStack(Items.EXPERIENCE_BOTTLE); c.setDelay(4);
            net.onestep.hybrid.ManagerTestDriver.exp(bridge,true, true, false, 20, 20); check(c.useCount == 0, "physical hold required");
            net.onestep.hybrid.ManagerTestDriver.exp(bridge,true, true, true, 1, 1);
            check(c.useCount == 1 && c.cooldownAtUse == 0, "single vanilla use after cooldown reset");
            net.onestep.hybrid.ManagerTestDriver.exp(bridge,true, true, true, 1, 1); check(c.useCount == 1, "interval respected");
            ready(bridge); net.onestep.hybrid.ManagerTestDriver.exp(bridge,true, true, true, 20, 20); check(c.useCount == 2, "next interval");
            c.player.main = new ItemStack(new Item()); ready(bridge); net.onestep.hybrid.ManagerTestDriver.exp(bridge,true, true, true, 20, 20);
            c.player.off = new ItemStack(Items.EXPERIENCE_BOTTLE); net.onestep.hybrid.ManagerTestDriver.exp(bridge,true, true, true, 20, 20);
            check(c.useCount == 2, "EXP exhaustion blocks until release");
            net.onestep.hybrid.ManagerTestDriver.exp(bridge,true, true, false, 20, 20); net.onestep.hybrid.ManagerTestDriver.exp(bridge,true, true, true, 20, 20);
            check(c.useCount == 3, "offhand EXP after release");
            c.level = new ClientLevel(); net.onestep.hybrid.ManagerTestDriver.exp(bridge,true, true, true, 20, 20);
            check(c.useCount == 4, "new world resets timer");
            c.screen = new Screen(); ready(bridge); net.onestep.hybrid.ManagerTestDriver.exp(bridge,true, true, true, 20, 20);
            check(c.useCount == 4, "screen blocked"); c.screen = null;
            net.onestep.hybrid.ManagerTestDriver.exp(bridge,true, false, true, 20, 20); check(c.useCount == 4, "focus blocked");
            net.onestep.hybrid.ManagerTestDriver.exp(bridge,false, true, true, 20, 20); check(c.useCount == 4, "disabled");
            c.failUse = true;
            try { net.onestep.hybrid.ManagerTestDriver.exp(bridge,true, true, true, 20, 20); throw new AssertionError("missing use error"); }
            catch (ReflectiveOperationException expected) {}
            check(c.useCount == 5, "partial use occurred exactly once");
            net.onestep.hybrid.ManagerTestDriver.exp(bridge,true, true, true, 20, 20); check(c.useCount == 5, "failure cannot replay");
            net.onestep.hybrid.ManagerTestDriver.exp(bridge,true, true, false, 20, 20); net.onestep.hybrid.ManagerTestDriver.exp(bridge,true, true, true, 20, 20);
            check(c.useCount == 6, "new hold after cleanup");
            Throwable[] failure = {null};
            Thread worker = new Thread(() -> { try { net.onestep.hybrid.ManagerTestDriver.exp(bridge,true, true, true, 50, 50); }
                catch (Throwable e) { failure[0] = e; } });
            ready(bridge); worker.start(); worker.join();
            check(failure[0] == null && c.useCount == 6, "wrong thread");
            bridge.stop(); net.onestep.hybrid.ManagerTestDriver.exp(bridge,true, true, true, 50, 50); check(c.useCount == 6, "stop");
        }
        System.out.println("PASS named Fast EXP controller (" + args[0] + ")");
    }
}
