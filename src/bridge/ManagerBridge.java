package net.onestep.hybrid;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** One parent-loader instance shared by all gameplay bridges. Wire strings always use UTF-8 byte arrays. */
public final class ManagerBridge {
    private ManagerBridge() {}
    private static volatile ModuleManager manager;
    private static ConfigManager configs;
    private static volatile long owner;
    private static int target;
    public static synchronized boolean start(long token, int abi, int build) {
        if (token == 0 || abi != ModuleManager.ABI || !Set.of(12111, 26100, 26101, 26102).contains(build)) return false;
        if (manager != null) return owner == token && target == build;
        manager = new ModuleManager(); configs = new ConfigManager(manager); owner = token; target = build;
        return true;
    }
    public static synchronized void stop(long token) {
        if (manager != null && token == owner) { manager.close(); manager = null; configs = null; owner = 0; target = 0; }
    }
    public static ModuleManager.Snapshot state() {
        ModuleManager current = manager;
        if (current == null) throw new IllegalStateException("Module manager is not initialized");
        return current.snapshot();
    }
    public static long revision() { ModuleManager current = manager; return current == null ? 0 : current.snapshot().revision(); }
    public static boolean current(String id, long generation) {
        ModuleManager current = manager; if (current == null) return false;
        var state = current.snapshot().module(id); return state.enabled() && state.generation() == generation;
    }
    public static int integer(ModuleManager.State state, String key) { return Json.integer(state.settings().get(key)); }
    public static boolean flag(ModuleManager.State state, String key) { return Json.bool(state.settings().get(key)); }
    public static float decimal(Object value) { return ((java.math.BigDecimal)value).floatValue(); }
    public static byte[] snapshot(long token, long previous) {
        ModuleManager current = manager;
        if (current == null || token != owner) return null;
        var snapshot = current.snapshot();
        if (snapshot.revision() == previous) return null;
        Map<String, Object> root = new LinkedHashMap<>(); root.put("revision", snapshot.revision());
        root.put("keybind_revision", snapshot.keybindRevision()); List<Object> entries = new ArrayList<>();
        for (var definition : ModuleCatalog.MODULES) {
            var state = snapshot.module(definition.id()); Map<String, Object> entry = new LinkedHashMap<>();
            entry.put("id", definition.id()); entry.put("name", definition.name()); entry.put("category", definition.category());
            entry.put("enabled", state.enabled()); entry.put("keybind", state.keybind()); entry.put("supports_keybind", definition.keybind());
            entry.put("generation", state.generation()); entry.put("settings", state.settings()); entries.add(entry);
        }
        root.put("modules", entries); return Json.bytes(root);
    }
    public static synchronized byte[] request(long token, byte[] bytes) {
        try {
            if (manager == null || token != owner) throw Json.error("Module session mismatch");
            var command = Json.object(Json.protocol(bytes)); String operation = Json.string(command.get("operation"));
            String id = command.containsKey("id") ? Json.string(command.get("id")) : "";
            ModuleManager.Result result;
            switch (operation) {
                case "enable" -> result = manager.setEnabled(id, Json.bool(command.get("enabled")));
                case "toggle" -> result = manager.toggle(id);
                case "keybind" -> result = manager.setKeybind(id, Json.integer(command.get("keybind")));
                case "reset" -> result = manager.reset(id);
                case "disable_all" -> result = manager.disableAll();
                case "settings", "validate" -> result = configs.patch(id, Json.object(command.get("settings")),
                    command.containsKey("expected_revision") ? ((java.math.BigDecimal)command.get("expected_revision")).longValueExact() : -1,
                    operation.equals("validate"));
                case "import", "import_keybinds" -> result = configs.importConfig(Json.string(command.get("payload")).getBytes(StandardCharsets.UTF_8), operation.equals("import_keybinds"));
                case "export", "export_keybinds" -> {
                    return response(new ModuleManager.Result(true, "Exported", manager.snapshot().revision()),
                        configs.exportConfig(operation.equals("export_keybinds")));
                }
                default -> throw Json.error("Unknown manager operation");
            }
            return response(result, null);
        } catch (IllegalArgumentException | ArithmeticException e) {
            return response(new ModuleManager.Result(false, e.getMessage(), revision()), null);
        }
    }
    private static byte[] response(ModuleManager.Result result, String payload) {
        Map<String, Object> output = new LinkedHashMap<>(); output.put("success", result.success());
        output.put("message", result.message()); output.put("revision", result.revision());
        if (payload != null) output.put("payload", payload);
        return Json.bytes(output);
    }
}
