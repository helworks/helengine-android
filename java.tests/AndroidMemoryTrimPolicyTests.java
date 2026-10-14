package com.helengine.android;

/** Tests the production notification threshold and low-severity Activity routing without native graphics. */
public final class AndroidMemoryTrimPolicyTests {
    /** Runs older running-pressure and modern background notification policy checks. */
    public static void main(String[] args) {
        for (int level : new int[]{-1,0,5,9}) {
            require(!AndroidMemoryTrimPolicy.shouldTrim(level),"Low-severity notification should not discard frame arenas.");
        }
        for (int level : new int[]{10,11,15,20,30,40,60,80,Integer.MAX_VALUE}) {
            require(AndroidMemoryTrimPolicy.shouldTrim(level),"Stronger, intermediate and future notification levels should trim.");
        }
        HelengineActivity activity = new HelengineActivity();
        activity.onTrimMemory(5);
        require(activity.LastTrimLevel == 5,"Activity must forward the superclass callback without invoking a native trim for low severity.");
        System.out.println("Passed 3 Android memory trim policy scenarios.");
    }
    /** Fails explicitly even when JVM assertions are disabled. */
    static void require(boolean condition,String message) { if (!condition) throw new AssertionError(message); }
}
