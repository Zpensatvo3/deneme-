package net.minecraft.world.item;
public class ItemStack {
    private final Item item;
    public ItemStack(Item item) { this.item = item; }
    public boolean isEmpty() { return item == null; }
    public Item getItem() { return item; }
}
