package net.onestep.hybrid;

import java.math.BigDecimal;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.CountDownLatch;

public final class ManagerCoreTest {
    private static int checks;
    private static void check(boolean condition, String message) { ++checks; if (!condition) throw new AssertionError(message); }
    private static byte[] bytes(String value) { return value.getBytes(StandardCharsets.UTF_8); }
    private static void reject(Runnable action, String message) {
        boolean rejected = false; try { action.run(); } catch (IllegalArgumentException e) { rejected = true; }
        check(rejected, message);
    }
    private static String entry(String id, boolean enabled, int keybind, String fields) {
        return "{\"id\":\"" + id + "\",\"enabled\":" + enabled + ",\"keybind\":" + keybind + ",\"settings\":" + fields + "}";
    }
    private static String document(String... entries) { return "{\"modules\":[" + String.join(",", entries) + "]}"; }
    public static void main(String[] args) throws Exception {
        ModuleManager manager = new ModuleManager(); ConfigManager configs = new ConfigManager(manager);
        check(ModuleCatalog.MODULES.size() == 36 && manager.snapshot().modules().size() == 36, "all 36 modules");
        check(manager.snapshot().module("anti_bot").enabled(), "anti bot default");
        check(manager.snapshot().module("watermark").enabled() && manager.snapshot().module("arraylist").enabled(), "HUD defaults");
        var held = manager.snapshot();
        manager.setEnabled("sprint", true);
        check(!held.module("sprint").enabled() && manager.snapshot().module("sprint").enabled(), "immutable publication");
        long unchanged = manager.snapshot().revision(); manager.setEnabled("sprint", true);
        check(manager.snapshot().revision() == unchanged, "no-op enabled request");
        check(configs.patch("fast_place", Json.object(Json.parse("{\"delay\":99}")), -1, false).success(), "numeric clamping");
        check(Json.integer(manager.snapshot().module("fast_place").settings().get("delay")) == 4, "clamped place delay");
        long before = manager.snapshot().revision();
        check(configs.patch("fast_place", Json.object(Json.parse("{\"delay\":4}")), -1, false).revision() == before, "no-op settings request");
        check(!configs.patch("fast_place", Map.of("delay", BigDecimal.ONE), before - 1, false).success(), "stale edit rejected");
        String exported = configs.exportConfig(false);
        long generation = manager.snapshot().module("fast_place").generation();
        check(configs.importConfig(bytes(exported), false).success(), "public round trip");
        check(manager.snapshot().module("fast_place").generation() == generation + 1, "identical import still resets lifecycle");
        check(configs.exportConfig(false).equals(exported), "stable export order/content");
        var atomic = manager.snapshot();
        String bad = document(entry("sprint", false, 65, "{}"), entry("jump_reset", true, 66, "{\"mode\":9}"));
        check(!configs.importConfig(bytes(bad), false).success() && manager.snapshot() == atomic, "invalid final entry publishes nothing");
        check(!configs.importConfig(bytes(document(entry("sprint", false, 1, "{}"), entry("sprint", true, 2, "{}"))), false).success(), "duplicate ID rejected");
        check(configs.importConfig(bytes(document(entry("removed_module", false, 0, "{}"), entry("sprint", false, 269, "{}"))), false).success(), "removed ID ignored / keypad enter");
        check(!configs.importConfig(bytes(document(entry("removed_module", false, 999, "{}"), entry("sprint", true, 0, "{}"))), false).success(), "removed IDs still structurally validated");
        for (int binding = 0; binding <= 255; ++binding) ModuleManager.validateKeybind(binding);
        reject(() -> ModuleManager.validateKeybind(256), "unsupported extended key rejected");
        reject(() -> ModuleManager.validateKeybind(-1), "negative key rejected");
        check(configs.importConfig(bytes(document(entry("fast_exp", true, 0, "{\"cps\":24}"))), false).success(), "legacy EXP migration");
        check(Json.integer(manager.snapshot().module("fast_exp").settings().get("min_cps")) == 22, "legacy EXP lower");
        check(Json.integer(manager.snapshot().module("fast_exp").settings().get("max_cps")) == 26, "legacy EXP upper");
        check(configs.importConfig(bytes(document(entry("triggerbot", true, 0,
            "{\"min_cooldown\":0.9,\"max_cooldown\":0.8,\"reaction_delay_min\":100,\"reaction_delay_max\":5,\"axe_only\":true,\"wait_critical\":true,\"on_left_click\":true}"))), false).success(), "legacy trigger migration");
        var trigger = manager.snapshot().module("triggerbot").settings();
        check(Json.integer(trigger.get("weapon")) == 2 && Json.integer(trigger.get("criticals")) == 2 && Json.bool(trigger.get("on_click")), "trigger aliases preserved");
        check(Json.integer(Json.array(trigger.get("reaction")).get(0)) == 5, "pair sorted");
        check(!configs.importConfig(bytes(document(entry("triggerbot", true, 0, "{\"sword_only\":true,\"axe_only\":true}"))), false).success(), "ambiguous legacy weapon rejected");
        check(!configs.importConfig(bytes(document(entry("triggerbot", true, 0, "{\"players\":false}"))), false).success(), "unrepresentable targets rejected");
        check(configs.importConfig(bytes(document(entry("blatant_click_aim_assist", true, 0, "{\"aim_point\":2}"))), false).success(), "blatant legacy enum");
        check(Json.integer(manager.snapshot().module("blatant_click_aim_assist").settings().get("aim_point")) == 4, "blatant enum converted");
        check(configs.importConfig(bytes(document(entry("auto_totem", false, 10, "{\"delay\":5}"), entry("legit_auto_totem", true, 11, "{}"))), false).success(), "totem aliases merged");
        var totem = manager.snapshot().module("auto_totem");
        check(totem.enabled() && totem.keybind() == 11 && Json.integer(totem.settings().get("mode")) == 1, "legit enabled selects random mode");
        check(configs.importConfig(bytes(document(entry("auto_totem", false, 10, "{\"mode\":0}"), entry("legit_auto_totem", true, 11, "{}"))), false).success(), "modern totem precedence");
        check(!manager.snapshot().module("auto_totem").enabled() && manager.snapshot().module("auto_totem").keybind() == 10, "modern entry wins even disabled");
        var sprint = manager.snapshot().module("sprint");
        check(configs.importConfig(bytes("{\"modules\":[{\"id\":\"sprint\",\"keybind\":45}]}"), true).success(), "keybind profile import");
        check(manager.snapshot().module("sprint").settings().equals(sprint.settings()) && manager.snapshot().module("sprint").enabled() == sprint.enabled(), "keybind import doesn't change gameplay");
        reject(() -> Json.parse("{\"x\":1,\"x\":2}"), "duplicate JSON key");
        reject(() -> Json.parse("[1,]"), "trailing comma");
        reject(() -> Json.parse("\"\\ud800\""), "unpaired surrogate");
        reject(() -> Json.parse(new byte[]{'"', (byte)0xc0, (byte)0xaf, '"'}), "invalid UTF8");
        check(Json.stringify(Json.parse("\"Kral \\ud83d\\udc3a Türkçe\"")).contains("Türkçe"), "UTF8 preserved");
        reject(() -> Json.parse(" ".repeat(Json.MAX_BYTES + 1)), "size bounded");
        reject(() -> Json.parse("[".repeat(34) + "0" + "]".repeat(34)), "depth bounded");
        reject(() -> Json.integer(Json.parse("1e0")), "lexical exponent not keybind");
        reject(() -> Json.integer(Json.parse("1.0")), "fraction not enum");
        for (var definition : ModuleCatalog.MODULES) {
            check(configs.patch(definition.id(), Map.of(), -1, true).success(), "valid defaults: " + definition.id());
            check(manager.snapshot().module(definition.id()).settings().keySet().equals(definition.defaults().keySet()), "schema coverage: " + definition.id());
        }
        var concurrent = new ModuleManager(); long revision = concurrent.snapshot().revision();
        CountDownLatch gate = new CountDownLatch(1); List<Thread> threads = new ArrayList<>();
        for (int i = 0; i < 4; ++i) {
            Thread thread = new Thread(() -> { try { gate.await(); for (int n = 0; n < 100; ++n) concurrent.toggle("sprint"); } catch (InterruptedException e) { throw new RuntimeException(e); } });
            threads.add(thread); thread.start();
        }
        gate.countDown(); for (Thread thread : threads) thread.join();
        check(concurrent.snapshot().revision() == revision + 400 && !concurrent.snapshot().module("sprint").enabled(), "racing writers serialize without lost toggles");
        concurrent.close(); reject(() -> concurrent.setEnabled("sprint", true), "closed session rejects mutation");
        for (int target : List.of(12111, 26100, 26101, 26102)) {
            check(!ManagerBridge.start(9, 999, target), "ABI mismatch closed");
            check(ManagerBridge.start(9, ModuleManager.ABI, target), "exact target handshake");
            check(!ManagerBridge.start(10, ModuleManager.ABI, target), "foreign owner rejected");
            byte[] initial = ManagerBridge.snapshot(9, 0); check(initial != null, "initial full snapshot");
            check(ManagerBridge.snapshot(9, ManagerBridge.revision()) == null, "unchanged snapshot allocates no wire document");
            String padded = "\n".repeat(20000) + document(entry("sprint", true, 0, "{}"));
            byte[] wire = Json.bytes(Map.of("operation", "import", "payload", padded));
            check(wire.length > Json.MAX_BYTES, "transport escaping expands config size");
            check(Json.bool(Json.object(Json.parse(ManagerBridge.request(9, wire))).get("success")), "96 KB limit applies to config, not escaped transport envelope");
            ManagerBridge.stop(10); check(ManagerBridge.revision() > 0, "foreign stop cannot stop owner");
            ManagerBridge.stop(9); check(ManagerBridge.revision() == 0, "owner session released");
        }
        check(!ManagerBridge.start(9, ModuleManager.ABI, 12108), "unsupported build rejected");
        System.out.println("Manager core: " + checks + " checks passed (36 modules, 4 targets)");
    }
}
