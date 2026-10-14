package android.os;
/** Retains the requested callback looper without creating desktop background threads. */
public final class Handler {
    /** Callback delivery owner selected by production code. */
    public final Looper looper;
    /** Captures the configured callback looper. */
    public Handler(Looper value) { looper = value; }
}
