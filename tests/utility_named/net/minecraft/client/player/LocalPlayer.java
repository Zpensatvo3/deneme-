package net.minecraft.client.player;
import net.minecraft.world.item.ItemStack;
public class LocalPlayer {
    public boolean eligible = true, using, sprinting, alive = true;
    private int noJumpDelay;
    public void setJumpDelay(int value) { noJumpDelay = value; }
    public int getJumpDelay() { return noJumpDelay; }
    public ItemStack main = new ItemStack(null), off = new ItemStack(null);
    private boolean canStartSprinting() { return eligible; }
    public boolean isAlive() { return alive; }
    public boolean isSprinting() { return sprinting; }
    public boolean isUsingItem() { return using; }
    public void setSprinting(boolean value) { sprinting = value; }
    public ItemStack getMainHandItem() { return main; }
    public ItemStack getOffhandItem() { return off; }
}
