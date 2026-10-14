package android.media;
import android.os.Handler;
/** Retains the exact request and listener so stale and synchronous callbacks can be exercised. */
public final class AudioFocusRequest {
    /** Requested focus lifetime. */
    public int gain;
    /** Media category matches the native output's client attributes. */
    public AudioAttributes attributes;
    /** Delayed requests may receive gain after the initial request returns. */
    public boolean delayed;
    /** Android must deliver a callback instead of automatically ducking this request. */
    public boolean pauseWhenDucked;
    /** Production listener associated with this request generation. */
    public AudioManager.OnAudioFocusChangeListener listener;
    /** Main-looper handler configured by production code. */
    public Handler handler;
    /** Mirrors Android's fluent request construction. */
    public static final class Builder {
        /** Accumulates request parameters. */
        final AudioFocusRequest value = new AudioFocusRequest();
        /** Captures the requested focus lifetime. */
        public Builder(int gain) { value.gain = gain; }
        /** Captures the media category. */
        public Builder setAudioAttributes(AudioAttributes attributes) { value.attributes = attributes; return this; }
        /** Captures permission to receive delayed gain. */
        public Builder setAcceptsDelayedFocusGain(boolean delayed) { value.delayed = delayed; return this; }
        /** Captures the pause-on-duck policy. */
        public Builder setWillPauseWhenDucked(boolean pause) { value.pauseWhenDucked = pause; return this; }
        /** Retains the real production listener and its chosen handler. */
        public Builder setOnAudioFocusChangeListener(AudioManager.OnAudioFocusChangeListener listener,Handler handler) { value.listener = listener; value.handler = handler; return this; }
        /** Returns the captured request. */
        public AudioFocusRequest build() { return value; }
    }
}
