package android.app;
import android.media.AudioManager;
/** Supplies a deterministic audio service to the production focus controller. */
public class Activity {
    /** Fake service records requests and owns injected callback timing. */
    public final AudioManager manager = new AudioManager();
    /** Mirrors typed Android service lookup. */
    public <T> T getSystemService(Class<T> type) { return type.cast(manager); }
}
