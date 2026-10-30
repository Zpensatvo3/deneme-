package net.onestep.hybrid;

import java.util.Map;

/** Fixtures configure the real manager, then exercise the shipping thin bridge entry points. */
public final class ManagerTestDriver {
    private static synchronized void configure(String id, boolean enabled, String fields) {
        if (ManagerBridge.revision() == 0 && !ManagerBridge.start(7, ModuleManager.ABI, 12111)) throw new AssertionError("Manager fixture session");
        String[] commands = {"{\"operation\":\"settings\",\"id\":\"" + id + "\",\"settings\":" + fields + "}",
            "{\"operation\":\"enable\",\"id\":\"" + id + "\",\"enabled\":" + enabled + "}"};
        for (String command : commands) {
            var result = Json.object(Json.parse(ManagerBridge.request(7, command.getBytes(java.nio.charset.StandardCharsets.UTF_8))));
            if (!Json.bool(result.get("success"))) throw new AssertionError(result.get("message"));
        }
    }
    public static void utility(UtilityBridge bridge, int feature, boolean enabled, int value, boolean held) throws ReflectiveOperationException {
        String id = feature == 1 ? "fast_place" : feature == 2 ? "sprint" : "full_bright";
        String settings = feature == 1 ? "{\"delay\":" + value + "}" : feature == 2 ? "{\"stop_while_using_item\":" + (value != 0) + "}" : "{}";
        configure(id, enabled, settings); bridge.update(feature, held);
    }
    public static void exp(ItemBridge bridge, boolean enabled, boolean permitted, boolean held, int low, int high) throws ReflectiveOperationException {
        configure("fast_exp", enabled, "{\"min_cps\":" + low + ",\"max_cps\":" + high + "}"); bridge.update(permitted, held);
    }
    public static void mouse(InventoryBridge bridge, boolean enabled, boolean right, boolean withItem, boolean withoutItem) throws ReflectiveOperationException {
        configure("mouse_tweaks", enabled, "{\"right_click_tweak\":" + right + ",\"left_click_with_item\":" + withItem + ",\"left_click_without_item\":" + withoutItem + "}");
        bridge.tickMouse();
    }
    public static boolean jumpDelay(GameBridge bridge) throws ReflectiveOperationException {
        configure("no_jump_delay", true, "{}"); return bridge.tick();
    }
    public static void trigger(WorldBridge bridge, boolean enabled, long generation, long manual, float low, float high,
        int reactionLow, int reactionHigh, int targets, int weapon, int criticals, boolean onClick, boolean swing, boolean simulate) throws ReflectiveOperationException {
        configure("triggerbot", enabled, "{\"cooldown\":[" + low + "," + high + "],\"reaction\":[" + reactionLow + "," + reactionHigh +
            "],\"targets\":" + targets + ",\"weapon\":" + weapon + ",\"criticals\":" + criticals + ",\"on_click\":" + onClick +
            ",\"swing_hand\":" + swing + ",\"click_simulation\":" + simulate + "}");
        bridge.tickTrigger(generation, manual);
    }
    public static void jump(WorldBridge bridge, boolean enabled, long generation, int mode, int chance, int delay, boolean combat) throws ReflectiveOperationException {
        configure("jump_reset", enabled, "{\"mode\":" + mode + ",\"chance\":" + chance + ",\"jump_delay_ms\":" + delay + ",\"only_combat\":" + combat + "}");
        bridge.tickJump(generation);
    }
    public static void totem(WorldBridge bridge, boolean enabled, long generation, int[] values, boolean autoOpen) throws ReflectiveOperationException {
        configure("auto_totem", enabled, "{\"mode\":" + values[0] + ",\"delay\":" + values[1] + ",\"hotbar\":" + (values[2] != 0) +
            ",\"totem_slot\":" + values[3] + ",\"auto_switch\":" + (values[4] != 0) + ",\"force_totem\":" + (values[5] != 0) +
            ",\"stay_open_for\":" + values[6] + ",\"auto_open\":" + autoOpen + "}");
        bridge.tickTotem(generation);
    }
}
