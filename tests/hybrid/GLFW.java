package org.lwjgl.glfw;

/** No native window is opened by the offline inventory bridge fixtures. */
public final class GLFW {
    public static final int[] buttons = new int[8], keys = new int[512];
    public static final int[] scans = new int[512];
    public static boolean focused = true;
    public static long lastWindow;
    public static int keyReads, mouseReads;
    static { reset(); }
    public static void reset() {
        java.util.Arrays.fill(buttons, 0); java.util.Arrays.fill(keys, 0);
        java.util.Arrays.fill(scans, -1); scans[32] = 0x39;
        focused = true; lastWindow = 0; keyReads = mouseReads = 0;
    }
    public static int glfwGetKeyScancode(int key) { return scans[key]; }
    public static int glfwGetWindowAttrib(long window, int attrib) { return focused ? 1 : 0; }
    public static int glfwGetMouseButton(long window, int button) { lastWindow = window; ++mouseReads; return buttons[button]; }
    public static int glfwGetKey(long window, int key) { lastWindow = window; ++keyReads; return keys[key]; }
}
