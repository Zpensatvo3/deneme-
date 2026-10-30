package net.minecraft;

import java.nio.file.*;
import java.util.*;
import java.util.regex.Pattern;
import net.onestep.hybrid.WorldBridge;
import net.onestep.hybrid.ClientTickRelay;

/** Real controller plus generated fake screen; never attaches to Minecraft. */
public final class AutoTotemBridgeTest {
    static int checks;
    static int[] values;
    static final class Bridge extends WorldBridge {
        boolean allowed=true;
        Bridge() throws Exception { super(AutoTotemBridgeTest.class.getClassLoader(),12111); installTickEvents(); }
        Bridge(int target,String[] bindings) throws Exception { super(AutoTotemBridgeTest.class.getClassLoader(),target,bindings); installTickEvents(); }
        @Override protected boolean totemPermitted(long generation) { return allowed; }
    }
    static void check(boolean ok,String what) { ++checks; if(!ok) throw new AssertionError(what); }
    static class_310 setup(boolean inventory) {
        class_310 c=new class_310(); class_310.field_1700=c;
        values=new int[]{0,0,1,1,0,0,0};
        if(inventory) c.field_1755=new class_490(c.field_1724);
        return c;
    }
    static void totem(class_310 c,int index) { c.field_1724.inventory.stacks[index]=new class_1799(class_1802.field_8288); }
    static boolean is(class_1799 s) { return s.item==class_1802.field_8288; }
    static void arm(Bridge b,boolean auto) throws Exception { net.onestep.hybrid.ManagerTestDriver.totem(b,true,1,values,auto); }
    static void tick() { ClientTickRelay.onTick(class_310.field_1700); }
    static String[] bindings() throws Exception {
        var match=Pattern.compile("\\{\"((?:net/minecraft/class_|field_|method_)[^\"]+)\", ").matcher(Files.readString(Path.of("src/services/game/world_bridge.cpp")));
        Set<String> keys=new TreeSet<>(); while(match.find()) keys.add(match.group(1));
        List<String> out=new ArrayList<>(); for(String key:keys) { out.add(key); out.add(key); }
        return out.toArray(String[]::new);
    }
    public static void main(String[] args) throws Exception {
        class_310 c=setup(true); totem(c,9); totem(c,10); Bridge b=new Bridge(); arm(b,false);
        check(c.field_1761.clicks==0,"render publishes settings without swapping"); tick();
        check(c.field_1761.clicks==1 && is(c.field_1724.offhand) && c.field_1761.source==9 && c.field_1761.destination==40,"first main-inventory source goes to offhand");
        tick(); check(c.field_1761.clicks==2 && is(c.field_1724.inventory.stacks[0]),"next tick fills selected hotbar");
        tick(); check(c.field_1755!=null && c.field_1761.clicks==2,"manual inventory stays open");
        c=setup(true); totem(c,0); totem(c,8); b=new Bridge(); arm(b,false); tick();
        check(c.field_1761.clicks==0,"hotbar never used as source");
        c=setup(true); totem(c,9); values[1]=2; b=new Bridge(); arm(b,false); tick();
        check(c.field_1761.clicks==0,"delay begins with tick countdown");
        for(int i=0;i<20;++i) arm(b,false);
        check(c.field_1761.clicks==0,"render frames do not advance delay"); tick();
        check(c.field_1761.clicks==1,"delay expires on second actual tick");
        c=setup(true); totem(c,9); totem(c,10); values[2]=0; b=new Bridge(); arm(b,false); tick(); tick();
        check(c.field_1761.clicks==1 && !is(c.field_1724.inventory.stacks[0]),"Hotbar off fills only offhand");
        c=setup(true); totem(c,9); c.field_1724.offhand=new class_1799(class_1802.field_8288);
        c.field_1724.inventory.stacks[0]=new class_1799(new class_1792()); b=new Bridge(); arm(b,false); tick();
        check(c.field_1761.clicks==0,"occupied main hand preserved without Force"); values[5]=1; arm(b,false); tick();
        check(c.field_1761.clicks==1 && is(c.field_1724.inventory.stacks[0]),"Force replaces occupied main hand");
        c=setup(true); totem(c,9); c.field_1724.offhand=new class_1799(class_1802.field_8288);
        values[3]=8; values[4]=1; b=new Bridge(); arm(b,false); tick();
        check(c.field_1724.inventory.field_7545==7 && c.field_1761.destination==7,"Auto Switch selects requested slot");
        c=setup(true); totem(c,9); c.field_1724.offhand=new class_1799(class_1802.field_8288);
        c.field_1724.inventory.field_7545=3; values[3]=8; b=new Bridge(); arm(b,false); tick();
        check(c.field_1761.destination==3 && !is(c.field_1724.inventory.stacks[7]),"without Auto Switch hotbar fills selected rather than configured slot");
        c=setup(false); totem(c,9); totem(c,10); values[6]=2; b=new Bridge(); arm(b,true); tick();
        check(c.field_1755 instanceof class_490 && c.field_1761.clicks==1,"Auto Open creates and services local inventory on same tick");
        class_490 fake=(class_490)c.field_1755;
        check(!fake.method_25402(1.,2.,0) && !fake.method_25402(new Object(),false),"fake screen blocks both mouse API shapes");
        fake.slotClick(); check(fake.clicks==0,"fake slot callback blocked");
        tick(); tick(); check(c.field_1755==fake,"Stay Open countdown holds generated screen");
        tick(); check(c.field_1755==fake,"Stay Open reaches zero without closing early");
        tick(); check(c.field_1755==null,"owned inventory closes after countdown");
        c=setup(false); totem(c,0); b=new Bridge(); arm(b,true); tick();
        check(c.field_1755==null,"Auto Open requires spare outside hotbar");
        Set<Integer> picked=new HashSet<>();
        Bridge randomBridge=new Bridge();
        var randomField=WorldBridge.class.getDeclaredField("totemRandom"); randomField.setAccessible(true);
        ((Random)randomField.get(randomBridge)).setSeed(27);
        for(int i=0;i<30;++i) { c=setup(true); values[0]=1; totem(c,12); totem(c,25); b=randomBridge; arm(b,false); tick(); picked.add(c.field_1761.source); }
        check(picked.equals(Set.of(12,25)),"Random samples only eligible sources");
        c=setup(true); totem(c,9); b=new Bridge(); b.allowed=false; arm(b,false); tick();
        check(c.field_1761.clicks==0,"permission blocks queued tick");
        c=setup(true); totem(c,9); b=new Bridge(); arm(b,false); net.onestep.hybrid.ManagerTestDriver.totem(b,false,2,values,false); tick();
        check(c.field_1761.clicks==0,"disable removes queued work");
        c=setup(true); totem(c,9); b=new Bridge(); arm(b,false); c.thread=false; tick();
        check(c.field_1761.clicks==0,"wrong thread cannot swap");
        c=setup(true); totem(c,9); b=new Bridge(); arm(b,false); c.field_1724.field_7512=new class_1703(); tick();
        check(c.field_1761.clicks==0,"screen/active handler mismatch cannot swap");
        for(int target:new int[]{12111,26100,26101,26102}) {
            c=setup(true); totem(c,9); b=new Bridge(target,bindings()); arm(b,false); tick();
            check(c.field_1761.clicks==1,"exact supplied target bindings "+target);
        }
        c=setup(true); totem(c,9); b=new Bridge(); arm(b,false); b.stop(); tick();
        check(c.field_1761.clicks==0,"stop detaches event callback");
        System.out.println("PASS Auto Totem: "+checks+" checks ("+args[0]+")");
    }
}
class class_310 {
    static class_310 field_1700;
    class_746 field_1724=new class_746(); class_638 field_1687=new class_638(); class_437 field_1755;
    class_315 field_1690=new class_315(); class_636 field_1761=new class_636(); boolean thread=true;
    public boolean method_18854() { return thread; }
    public void method_1507(class_437 screen) { field_1755=screen; }
}
class class_746 extends class_1657 {}
class class_638 {} class class_315 {} class class_1297 {}
class class_437 { int clicks; public void method_25419() { class_310.field_1700.field_1755=null; } }
class class_465 extends class_437 { class_1703 field_2797; }
class class_1661 {
    int field_7545; class_1799[] stacks=new class_1799[36];
    class_1661() { Arrays.setAll(stacks,i -> new class_1799(null)); }
    public class_1799 method_5438(int i) { return stacks[i]; }
}
class class_1792 {} class class_1802 { static class_1792 field_8288=new class_1792(); }
class class_1799 {
    final class_1792 item; class_1799(class_1792 i) { item=i; }
    public boolean method_7960() { return item==null; } public class_1792 method_7909() { return item; }
}
class class_1703 { int field_7763=0; }
class class_1713 { static class_1713 field_7791=new class_1713(); } class class_1735 {}
class class_636 {
    int clicks,source=-1,destination=-1;
    // Base PlayerEntity is intentional: using ClientPlayerEntity in lookup must fail.
    public void method_2906(int sync,int slot,int button,class_1713 action,class_1657 player) {
        ++clicks; source=slot; destination=button;
        class_1799 from=player.inventory.stacks[slot];
        if(button==40) { player.inventory.stacks[slot]=player.offhand; player.offhand=from; }
        else { player.inventory.stacks[slot]=player.inventory.stacks[button]; player.inventory.stacks[button]=from; }
    }
}
