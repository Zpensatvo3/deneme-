package net.onestep.hybrid;
import java.util.function.Consumer;

/** Shared client-thread tick delivery for Auto Totem. */
public final class ClientTickRelay {
    private static volatile Object owner;
    private static volatile Consumer<Object> tick;
    public static synchronized void install(Object token, Consumer<Object> callback) {
        if (token == null || callback == null) throw new IllegalArgumentException("Client tick relay");
        tick = null; owner = token; tick = callback;
    }
    public static synchronized void stop(Object token) {
        if (owner != token) return;
        tick = null; owner = null;
    }
    public static void onTick(Object client) {
        Consumer<Object> callback = tick;
        if (callback != null) callback.accept(client);
    }
}
