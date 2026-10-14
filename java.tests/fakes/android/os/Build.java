package android.os;
/** Exposes the runtime API selected by each focus contract test. */
public final class Build {
    /** Mirrors Android's runtime API field. */
    public static final class VERSION {
        /** Defaults to the current host target; legacy tests temporarily choose API 26. */
        public static int SDK_INT = 35;
    }
}
