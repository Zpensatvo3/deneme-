package net.minecraft;
public class class_490 extends class_465 {
    public class_490(class_1657 player) { field_2797=player.field_7512; }
    protected void method_2383(class_1735 slot,int id,int button,class_1713 action) { clicks++; }
    public boolean method_25402(double x,double y,int button) { clicks++; return true; }
    public boolean method_25402(Object click,boolean doubled) { clicks++; return true; }
    public void slotClick() { method_2383(null,1,0,class_1713.field_7791); }
}
