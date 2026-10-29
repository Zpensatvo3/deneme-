package net.minecraft.client;
import net.minecraft.client.gui.screens.Screen;
import net.minecraft.client.multiplayer.ClientLevel;
import net.minecraft.client.player.LocalPlayer;
public class Minecraft {
    private static Minecraft instance;
    private int rightClickDelay;
    public int useCount, cooldownAtUse;
    public boolean failUse;
    public final Options options = new Options();
    public ClientLevel level = new ClientLevel();
    public LocalPlayer player = new LocalPlayer();
    public Screen screen;
    private final Thread thread = Thread.currentThread();
    public Minecraft() { instance = this; }
    public void setDelay(int value) { rightClickDelay = value; }
    public int getDelay() { return rightClickDelay; }
    private void startUseItem() {
        cooldownAtUse = rightClickDelay; ++useCount; rightClickDelay = 4;
        if (failUse) { failUse = false; throw new IllegalStateException("use failure"); }
    }
    public boolean isSameThread() { return Thread.currentThread() == thread; }
}
