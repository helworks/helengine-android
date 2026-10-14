package com.helengine.android;
import java.util.ArrayList;
import java.util.List;
/** Records permission changes emitted by the real production focus controller. */
public final class FocusRecorder implements AudioFocusObserver {
    /** Last permission published to the native mixer. */
    boolean granted;
    /** Ordered permission changes reveal accidental grants or duplicate events. */
    final List<Boolean> changes = new ArrayList<>();
    /** Records a controller publication. */
    @Override public void onAudioFocusChanged(boolean value) { granted = value; changes.add(value); }
}
