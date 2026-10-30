package net.onestep.hybrid;

import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Set;

/** Session-scoped state owner. Readers capture one immutable publication; no game callbacks run under its lock. */
public final class ModuleManager {
    public static final int ABI = 1;
    public record State(boolean enabled, int keybind, Map<String, Object> settings, long generation) {
        public State { settings = Collections.unmodifiableMap(new LinkedHashMap<>(settings)); }
    }
    public record Snapshot(long revision, long keybindRevision, Map<String, State> modules) {
        public Snapshot { modules = Collections.unmodifiableMap(new LinkedHashMap<>(modules)); }
        public State module(String id) {
            State result = modules.get(id);
            if (result == null) throw Json.error("Unknown module: " + id);
            return result;
        }
    }
    public record Result(boolean success, String message, long revision) {}
    private volatile Snapshot snapshot;
    private boolean active = true;
    public ModuleManager() {
        Map<String, State> initial = new LinkedHashMap<>();
        for (var definition : ModuleCatalog.MODULES)
            initial.put(definition.id(), new State(definition.enabled(), 0,
                ConfigManager.normalize(definition.id(), definition.defaults(), Map.of()), 0));
        snapshot = new Snapshot(1, 0, initial);
    }
    public Snapshot snapshot() { return snapshot; }
    public synchronized Result setEnabled(String id, boolean value) {
        requireActive(); State old = snapshot.module(id);
        return publish(Map.of(id, new State(value, old.keybind(), old.settings(), old.generation())), Set.of());
    }
    public synchronized Result toggle(String id) { return setEnabled(id, !snapshot.module(id).enabled()); }
    public synchronized Result setKeybind(String id, int value) {
        requireActive(); validateKeybind(value);
        var definition = ModuleCatalog.BY_ID.get(id); State old = snapshot.module(id);
        if (!definition.keybind()) throw Json.error("This module has a fixed shortcut");
        return publish(Map.of(id, new State(old.enabled(), value, old.settings(), old.generation())), Set.of());
    }
    public synchronized Result reset(String id) { requireActive(); snapshot.module(id); return publish(Map.of(), Set.of(id)); }
    public synchronized Result disableAll() {
        requireActive(); Map<String, State> pending = new LinkedHashMap<>();
        snapshot.modules().forEach((id, old) -> pending.put(id, new State(false, old.keybind(), old.settings(), old.generation())));
        return publish(pending, Set.of());
    }
    public synchronized void close() { if (active) { disableAll(); active = false; } }
    synchronized void requireActive() { if (!active) throw Json.error("Module session is stopped"); }
    synchronized Result publish(Map<String, State> pending, Set<String> resets) {
        requireActive(); Snapshot before = snapshot;
        Map<String, State> candidate = new LinkedHashMap<>(before.modules());
        boolean changed = false, bindingsChanged = false;
        for (String id : before.modules().keySet()) {
            State old = before.module(id), next = pending.getOrDefault(id, old);
            boolean reset = resets.contains(id) || old.enabled() != next.enabled() || !old.settings().equals(next.settings());
            boolean binding = old.keybind() != next.keybind();
            if (reset || binding) {
                changed = true; bindingsChanged |= binding;
                candidate.put(id, new State(next.enabled(), next.keybind(), next.settings(), old.generation() + (reset ? 1 : 0)));
            }
        }
        if (changed) snapshot = new Snapshot(before.revision() + 1, before.keybindRevision() + (bindingsChanged ? 1 : 0), candidate);
        return new Result(true, changed ? "State updated" : "Unchanged", snapshot.revision());
    }
    public static void validateKeybind(int value) {
        // Windows VKs plus the existing keypad Enter extension.
        if (!(value >= 0 && value <= 255 || value == 0x10d))
            throw Json.error("Invalid keybind");
    }
}
