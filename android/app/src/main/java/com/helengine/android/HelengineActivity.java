package com.helengine.android;

import android.app.NativeActivity;
import android.os.Bundle;

/** Keeps NativeActivity's Vulkan host and supplies Android audio-focus lifecycle callbacks. */
public final class HelengineActivity extends NativeActivity {
    /** Registers this app's JNI callbacks with its class loader before lifecycle listeners can invoke them. */
    static { System.loadLibrary("helengine_android"); }

    /** Controller is created before NativeActivity starts the native glue thread. */
    AndroidAudioFocus AudioFocus;
    /** Java lifecycle gate prevents a delayed native resume from requesting focus during suspension. */
    volatile boolean Resumed;

    /** Prepares the focus bridge before the native entry point creates the game host. */
    @Override protected void onCreate(Bundle savedInstanceState) {
        AudioFocus = new AndroidAudioFocus(this, this::nativeSetAudioFocusGranted);
        super.onCreate(savedInstanceState);
    }

    /** Makes focus requests eligible before NativeActivity delivers the resume command. */
    @Override protected void onResume() {
        synchronized (AudioFocus) { Resumed = true; }
        super.onResume();
    }

    /** Mutes immediately on the UI thread before the native loop processes suspension. */
    @Override protected void onPause() {
        synchronized (AudioFocus) {
            Resumed = false;
            AudioFocus.setActive(false);
        }
        super.onPause();
    }

    /** Retires focus before NativeActivity waits for native resource destruction. */
    @Override protected void onDestroy() {
        synchronized (AudioFocus) {
            Resumed = false;
            AudioFocus.setActive(false);
        }
        super.onDestroy();
    }

    /** Called by the native engine thread only while a resumed host has a drawable window. */
    public void setGameAudioFocusActive(boolean active) {
        synchronized (AudioFocus) { AudioFocus.setActive(active && Resumed); }
    }

    /** Publishes memory pressure to the host queue; Vulkan cleanup never runs on the UI callback thread. */
    @Override public void onTrimMemory(int level) {
        super.onTrimMemory(level);
        if (AndroidMemoryTrimPolicy.shouldTrim(level)) nativeRequestMemoryTrim(level);
    }

    /** Requests deferred cleanup through a live-activity registration without storing a native Java-side pointer. */
    native void nativeRequestMemoryTrim(int level);

    /** Publishes a gate change through an activity-checked native registration, never a Java-stored native pointer. */
    native void nativeSetAudioFocusGranted(boolean granted);
}
