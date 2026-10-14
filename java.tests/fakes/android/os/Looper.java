package android.os;
/** Supplies one main-thread identity for the focus listener handler. */
public final class Looper {
    /** Stable main looper shared by all requests. */
    static final Looper Main = new Looper();
    /** Returns the Android-shaped main looper handle. */
    public static Looper getMainLooper() { return Main; }
}
