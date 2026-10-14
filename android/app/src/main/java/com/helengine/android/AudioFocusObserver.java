package com.helengine.android;

/** Receives permission changes independently of the activity's lifecycle or the game's mixer buses. */
public interface AudioFocusObserver {
    /** Publishes whether the current focus request permits game audio. */
    void onAudioFocusChanged(boolean granted);
}
