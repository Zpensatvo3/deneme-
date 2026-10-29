import net.onestep.visual.SkyBridge;

public final class RetiredBridgeTargetsTest {
    interface Action { void run() throws Exception; }
    static void rejected(Action action) throws Exception {
        try { action.run(); } catch (IllegalArgumentException expected) { return; }
        throw new AssertionError("Retired target accepted");
    }
    public static void main(String[] args) throws Exception {
        ClassLoader loader = RetiredBridgeTargetsTest.class.getClassLoader();
        for (int target : new int[]{0, 12101, 12104, 12108, 26103}) {
            rejected(() -> SkyBridge.install(loader, target));
        }
        System.out.println("PASS retired bridge targets rejected");
    }
}
