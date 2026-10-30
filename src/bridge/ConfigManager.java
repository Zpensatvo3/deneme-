package net.onestep.hybrid;

import java.math.BigDecimal;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Validation, migrations and atomic configuration transactions for every module. */
public final class ConfigManager {
    private final ModuleManager manager;
    public ConfigManager(ModuleManager manager) { this.manager = java.util.Objects.requireNonNull(manager); }
    public String exportConfig(boolean keybindsOnly) {
        var snapshot = manager.snapshot(); List<Object> modules = new ArrayList<>();
        for (var definition : ModuleCatalog.MODULES) {
            if (keybindsOnly && !definition.keybind()) continue;
            var state = snapshot.module(definition.id()); Map<String, Object> entry = new LinkedHashMap<>();
            entry.put("id", definition.id());
            if (!keybindsOnly) entry.put("enabled", state.enabled());
            entry.put("keybind", state.keybind());
            if (!keybindsOnly) entry.put("settings", state.settings());
            modules.add(entry);
        }
        return Json.stringify(Map.of("modules", modules));
    }
    public ModuleManager.Result importConfig(byte[] payload, boolean keybindsOnly) {
        synchronized (manager) {
            try {
                manager.requireActive(); var before = manager.snapshot();
                var root = Json.object(Json.parse(payload));
                if (!keybindsOnly && root.containsKey("version") && Json.integer(root.get("version")) != 3)
                    throw Json.error("Unsupported legacy config format");
                List<Object> entries = Json.array(root.get("modules"));
                if (entries.size() > 128) throw Json.error("At most 128 module entries are allowed");
                Map<String, ModuleManager.State> pending = new LinkedHashMap<>(); Set<String> seen = new HashSet<>();
                Set<String> resets = new HashSet<>(); Map<String, Object> normal = null, legit = null;
                for (Object raw : entries) {
                    var entry = Json.object(raw); String storedId = Json.string(entry.get("id"));
                    // Keep existing public configs and local bindings after the module rename.
                    String id = storedId.equals("blatant_click_aim_assist") ? "click_aim_assist" : storedId;
                    if (id.isEmpty() || id.getBytes(StandardCharsets.UTF_8).length > 128 || !seen.add(id))
                        throw Json.error("Invalid or duplicate module ID: " + id);
                    int binding = Json.integer(entry.get("keybind")); ModuleManager.validateKeybind(binding);
                    boolean enabled = !keybindsOnly && Json.bool(entry.get("enabled"));
                    Map<String, Object> patch = keybindsOnly ? Map.of() : Json.object(entry.get("settings"));
                    if (!keybindsOnly && (id.equals("auto_totem") || id.equals("legit_auto_totem"))) {
                        // Validate both aliases independently, including an alias later overridden by the modern entry.
                        Map<String, Object> converted = new LinkedHashMap<>();
                        patch.forEach((key, value) -> converted.put(id.equals("legit_auto_totem") ? "legit_" + key : key, value));
                        normalize("auto_totem", before.module("auto_totem").settings(), converted);
                        if (id.equals("auto_totem")) normal = entry; else legit = entry;
                        continue;
                    }
                    var definition = ModuleCatalog.BY_ID.get(id);
                    if (definition == null || keybindsOnly && !definition.keybind()) continue;
                    var old = before.module(id);
                    pending.put(id, new ModuleManager.State(keybindsOnly ? old.enabled() : enabled,
                        binding,
                        keybindsOnly ? old.settings() : normalize(id, old.settings(), patch), old.generation()));
                    if (!keybindsOnly) resets.add(id);
                }
                if (normal != null || legit != null) {
                    Map<String, Object> patch = new LinkedHashMap<>();
                    if (legit != null) Json.object(legit.get("settings")).forEach((key, value) -> patch.put("legit_" + key, value));
                    Map<String, Object> normalSettings = normal == null ? Map.of() : Json.object(normal.get("settings"));
                    boolean modern = normalSettings.keySet().stream().anyMatch(key -> key.equals("mode") || key.startsWith("legit_"));
                    patch.putAll(normalSettings);
                    var selected = normal;
                    if (!modern && (normal == null || !Json.bool(normal.get("enabled")) && legit != null && Json.bool(legit.get("enabled")))) selected = legit;
                    if (selected == null) selected = legit;
                    if (!modern) patch.put("mode", BigDecimal.valueOf(selected == legit ? 1 : 0));
                    var old = before.module("auto_totem");
                    pending.put("auto_totem", new ModuleManager.State(Json.bool(selected.get("enabled")), Json.integer(selected.get("keybind")),
                        normalize("auto_totem", old.settings(), patch), old.generation()));
                    resets.add("auto_totem");
                }
                if (!keybindsOnly && pending.isEmpty()) throw Json.error("This config has no compatible modules");
                var result = manager.publish(pending, resets);
                return new ModuleManager.Result(true, keybindsOnly ? "Local keybinds loaded." : pending.size() + " module settings loaded.", result.revision());
            } catch (IllegalArgumentException e) { return failure(e); }
        }
    }
    public ModuleManager.Result patch(String id, Map<String, Object> fields, long expectedRevision, boolean validateOnly) {
        synchronized (manager) {
            try {
                manager.requireActive(); var before = manager.snapshot(); var old = before.module(id);
                if (Json.bytes(fields).length > Json.MAX_BYTES) throw Json.error("Module settings exceed 96 KB");
                if (expectedRevision >= 0 && expectedRevision != before.revision()) throw Json.error("State changed; retry the edit");
                var candidate = normalize(id, old.settings(), fields);
                if (validateOnly) return new ModuleManager.Result(true, "Valid settings", before.revision());
                return manager.publish(Map.of(id, new ModuleManager.State(old.enabled(), old.keybind(), candidate, old.generation())), Set.of());
            } catch (IllegalArgumentException e) { return failure(e); }
        }
    }
    private ModuleManager.Result failure(Exception e) { return new ModuleManager.Result(false, e.getMessage(), manager.snapshot().revision()); }
    public static Map<String, Object> normalize(String id, Map<String, Object> previous, Map<String, Object> fields) {
        var definition = ModuleCatalog.BY_ID.get(id);
        if (definition == null) throw Json.error("Unknown module: " + id);
        Map<String, Object> patch = new LinkedHashMap<>(fields);
        migrate(id, previous, patch);
        Map<String, Object> candidate = new LinkedHashMap<>(previous);
        for (var field : definition.fields()) {
            if (patch.containsKey(field.key())) candidate.put(field.key(), normalizeField(field, patch.get(field.key()), candidate));
        }
        if (id.equals("fast_exp")) {
            int legacy = Json.integer(candidate.get("cps"));
            if (legacy > 0 && Json.integer(candidate.get("min_cps")) == 16 && Json.integer(candidate.get("max_cps")) == 20) {
                candidate.put("min_cps", BigDecimal.valueOf(Math.max(5, legacy - 2)));
                candidate.put("max_cps", BigDecimal.valueOf(Math.min(40, legacy + 2)));
            }
            candidate.put("cps", BigDecimal.ZERO);
        }
        // Revisit in schema order, so dependent lower bounds observe normalized earlier fields.
        for (var field : definition.fields()) candidate.put(field.key(), normalizeField(field, candidate.get(field.key()), candidate));
        if (id.equals("click_aim_assist") && Json.array(candidate.get("aim_regions")).size() > 10)
            throw Json.error("Aim Regions: select at most 10 of the 60 regions");
        return Collections.unmodifiableMap(candidate);
    }
    private static Object normalizeField(ModuleCatalog.Field field, Object value, Map<String, Object> context) {
        int kind = field.kind();
        if (kind == 0) return Json.bool(value);
        if (kind == 9) {
            List<Object> list = Json.array(value);
            if (list.size() > 256) throw Json.error(field.key() + ": list exceeds 256 entries");
            List<Integer> values = new ArrayList<>(); Set<Integer> seen = new HashSet<>();
            for (Object item : list) {
                int selected = Json.integer(item);
                if (selected < field.minimum() || selected > field.maximum() || !seen.add(selected))
                    throw Json.error(field.key() + ": invalid or duplicate region ID");
                values.add(selected);
            }
            Collections.sort(values);
            return values.stream().map(selected -> BigDecimal.valueOf(selected.longValue())).toList();
        }
        if (kind == 8) {
            List<Object> list = Json.array(value);
            if (list.size() > 256) throw Json.error(field.key() + ": list exceeds 256 entries");
            for (Object item : list) if (Json.string(item).getBytes(StandardCharsets.UTF_8).length > 128) throw Json.error(field.key() + ": string exceeds 128 bytes");
            return List.copyOf(list);
        }
        if (kind == 3) {
            int selected = Json.integer(value);
            if (!field.choices().contains(selected)) throw Json.error(field.key() + ": invalid enum value");
            return BigDecimal.valueOf(selected);
        }
        double low = field.minimum(), high = field.maximum();
        if (field.minimumFrom() != null) low = Math.max(low, Math.min(high, Json.integer(context.get(field.minimumFrom()))));
        if (kind == 5 || kind == 6 || kind == 7) {
            List<Object> raw = Json.array(value); int count = kind == 5 ? 4 : 2;
            if (raw.size() != count) throw Json.error(field.key() + ": invalid array length");
            List<BigDecimal> result = new ArrayList<>();
            for (Object item : raw) result.add(number(item, low, high, kind == 7));
            if (count == 2 && result.get(0).compareTo(result.get(1)) > 0) Collections.swap(result, 0, 1);
            return List.copyOf(result);
        }
        return number(value, low, high, kind == 4);
    }
    private static BigDecimal number(Object value, double low, double high, boolean integer) {
        if (!(value instanceof BigDecimal number) || number.abs().compareTo(BigDecimal.valueOf(1000000)) > 0)
            throw Json.error("Number must be finite and within +/-1000000");
        if (integer) Json.integer(value);
        double clamped = Math.max(low, Math.min(high, number.doubleValue()));
        // Native models use float, so canonicalize decimals to their round-trippable float representation.
        return integer ? BigDecimal.valueOf((int)clamped) : new BigDecimal(Float.toString((float)clamped));
    }
    private static void migrate(String id, Map<String, Object> previous, Map<String, Object> fields) {
        if (id.equals("click_aim_assist") && fields.containsKey("aim_point") && !fields.containsKey("aim_regions")) {
            int point = Json.integer(fields.get("aim_point"));
            if (point < 0 || point > 4) throw Json.error("Invalid legacy aim point");
            List<BigDecimal> regions = new ArrayList<>();
            for (int region = point == 0 ? 0 : 10; region < (point == 0 ? 10 : 20); ++region)
                regions.add(BigDecimal.valueOf(region));
            fields.put("aim_regions", regions);
        }
        if (!id.equals("triggerbot")) return;
        pair(fields, previous, "cooldown", "min_cooldown", "max_cooldown");
        pair(fields, previous, "reaction", "reaction_delay_min", "reaction_delay_max");
        if (!fields.containsKey("weapon") && (fields.containsKey("sword_only") || fields.containsKey("axe_only"))) {
            boolean sword = flag(fields, "sword_only", false), axe = flag(fields, "axe_only", false);
            if (sword && axe) throw Json.error("Triggerbot: sword_only and axe_only cannot both be enabled");
            fields.put("weapon", BigDecimal.valueOf(sword ? 1 : axe ? 2 : 0));
        }
        if (!fields.containsKey("targets") && (fields.containsKey("players") || fields.containsKey("mobs") || fields.containsKey("animals"))) {
            int old = Json.integer(previous.get("targets"));
            boolean players = flag(fields, "players", true), mobs = flag(fields, "mobs", old >= 1), animals = flag(fields, "animals", old == 2);
            if (!players || animals && !mobs) throw Json.error("Triggerbot: legacy target selection cannot be represented safely");
            fields.put("targets", BigDecimal.valueOf(animals ? 2 : mobs ? 1 : 0));
        }
        int criticals = Json.integer(fields.getOrDefault("criticals", previous.get("criticals")));
        if (!fields.containsKey("criticals") && fields.containsKey("wait_critical")) criticals = Json.bool(fields.get("wait_critical")) ? 2 : 0;
        if (flag(fields, "prioritize_crits", false) && criticals == 0) criticals = 1;
        fields.put("criticals", BigDecimal.valueOf(criticals));
        if (!fields.containsKey("on_click") && fields.containsKey("on_left_click")) fields.put("on_click", fields.get("on_left_click"));
    }
    private static boolean flag(Map<String, Object> fields, String key, boolean fallback) { return fields.containsKey(key) ? Json.bool(fields.get(key)) : fallback; }
    private static void pair(Map<String, Object> fields, Map<String, Object> previous, String key, String low, String high) {
        if (fields.containsKey(key)) return;
        var old = Json.array(previous.get(key));
        fields.put(key, List.of(fields.getOrDefault(low, old.get(0)), fields.getOrDefault(high, old.get(1))));
    }
}
