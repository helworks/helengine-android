package com.helengine.android;

/** Selects notifications that justify releasing rebuildable renderer storage without changing gameplay quality. */
public final class AndroidMemoryTrimPolicy {
    /** Historical RUNNING_LOW level; higher values also include modern UI_HIDDEN and BACKGROUND notifications. */
    static final int MinimumTrimLevel = 10;
    /** This class exposes a pure notification policy and has no per-activity state. */
    private AndroidMemoryTrimPolicy() {}
    /** Uses an ordinal threshold so intermediate or future stronger levels receive the same cleanup. */
    public static boolean shouldTrim(int level) { return level >= MinimumTrimLevel; }
}
