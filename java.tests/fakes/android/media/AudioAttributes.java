package android.media;
/** Retains the focus request attributes for cross-API contract assertions. */
public final class AudioAttributes {
    /** Platform usage and content constants consumed by production code. */
    public static final int USAGE_MEDIA=1, USAGE_GAME=14, CONTENT_TYPE_MUSIC=2;
    /** Requested media category. */
    public int usage;
    /** Requested content category. */
    public int contentType;
    /** Mirrors Android's fluent attribute construction. */
    public static final class Builder {
        /** Accumulates attributes until build. */
        final AudioAttributes value = new AudioAttributes();
        /** Records the media category. */
        public Builder setUsage(int usage) { value.usage = usage; return this; }
        /** Records the content category. */
        public Builder setContentType(int content) { value.contentType = content; return this; }
        /** Returns the captured attributes. */
        public AudioAttributes build() { return value; }
    }
}
