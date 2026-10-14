package android.media;
import java.util.ArrayList;
import java.util.List;
/** Injects focus outcomes and callback timing without depending on an Android device. */
public final class AudioManager {
    /** Platform focus events and request results consumed by production code. */
    public static final int AUDIOFOCUS_GAIN=1, AUDIOFOCUS_LOSS=-1, AUDIOFOCUS_LOSS_TRANSIENT=-2,
        AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK=-3, AUDIOFOCUS_REQUEST_FAILED=0, AUDIOFOCUS_REQUEST_GRANTED=1,
        AUDIOFOCUS_REQUEST_DELAYED=2;
    /** Mirrors Android's listener contract. */
    public interface OnAudioFocusChangeListener {
        /** Delivers one injected system focus event. */
        void onAudioFocusChange(int change);
    }
    /** Requests are retained so old listeners can be invoked deliberately. */
    public final List<AudioFocusRequest> requests = new ArrayList<>();
    /** Exact abandoned request identities are checked by the tests. */
    public final List<AudioFocusRequest> abandoned = new ArrayList<>();
    /** Return value injected for the next activation. */
    public int result = AUDIOFOCUS_REQUEST_GRANTED;
    /** Optional callback delivered before requestAudioFocus returns. */
    public Integer synchronousChange;
    /** Simulates a queued gain arriving while abandonment is in progress. */
    public boolean gainDuringAbandon;
    /** Captures and optionally calls back a new request before returning its configured result. */
    public int requestAudioFocus(AudioFocusRequest request) {
        requests.add(request);
        if (synchronousChange != null) request.listener.onAudioFocusChange(synchronousChange);
        return result;
    }
    /** Captures the exact retired request and optionally delivers its now-stale callback. */
    public int abandonAudioFocusRequest(AudioFocusRequest request) {
        abandoned.add(request);
        if (gainDuringAbandon) request.listener.onAudioFocusChange(AUDIOFOCUS_GAIN);
        return AUDIOFOCUS_REQUEST_GRANTED;
    }
}
