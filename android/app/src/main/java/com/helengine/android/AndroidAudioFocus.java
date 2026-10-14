package com.helengine.android;

import android.app.Activity;
import android.media.AudioAttributes;
import android.media.AudioFocusRequest;
import android.media.AudioManager;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;

/** Requests game audio focus and rejects callbacks belonging to abandoned or replaced requests. */
public final class AndroidAudioFocus {
    /** Android service owns routing and competing applications' focus requests. */
    final AudioManager Manager;
    /** Native host receives changes without accessing a retired native pointer from Java. */
    final AudioFocusObserver Observer;
    /** Callbacks run on Android's main looper rather than the real-time audio thread. */
    final Handler CallbackHandler;
    /** Exact request instance must be used when abandoning focus. */
    AudioFocusRequest Request;
    /** Host's current request to participate in audio focus. */
    boolean Active;
    /** Permission last published to the native audio gate. */
    boolean Granted;
    /** Invalidates callbacks from retired requests, including callbacks already queued on the main looper. */
    long Generation;
    /** Detects a callback delivered synchronously during requestAudioFocus. */
    long ChangeRevision;

    /** Binds the service and observer while the activity is alive. */
    public AndroidAudioFocus(Activity activity, AudioFocusObserver observer) {
        Manager = activity.getSystemService(AudioManager.class);
        if (Manager == null || observer == null) throw new IllegalArgumentException("Audio focus requires a service and observer.");
        Observer = observer;
        CallbackHandler = new Handler(Looper.getMainLooper());
    }

    /** Requests once per activation; suspension abandons focus and cannot be undone by a stale gain callback. */
    public synchronized void setActive(boolean active) {
        if (Active == active) return;
        Active = active;
        if (!active) {
            retireRequest();
            return;
        }
        final long generation = ++Generation;
        final long revision = ChangeRevision;
        Request = new AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN)
            .setAudioAttributes(new AudioAttributes.Builder()
                .setUsage(Build.VERSION.SDK_INT >= 28 ? AudioAttributes.USAGE_GAME : AudioAttributes.USAGE_MEDIA)
                .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC).build())
            .setAcceptsDelayedFocusGain(true)
            .setWillPauseWhenDucked(true)
            .setOnAudioFocusChangeListener(change -> onFocusChange(generation, change), CallbackHandler)
            .build();
        int result = Manager.requestAudioFocus(Request);
        if (generation != Generation || revision != ChangeRevision) return;
        if (result == AudioManager.AUDIOFOCUS_REQUEST_GRANTED) {
            publish(true);
        } else if (result != AudioManager.AUDIOFOCUS_REQUEST_DELAYED) {
            retireRequest();
        }
    }

    /** Applies only the current request's changes; permanent loss requires a later host activation. */
    synchronized void onFocusChange(long generation, int change) {
        if (!Active || Request == null || generation != Generation) return;
        ++ChangeRevision;
        switch (change) {
            case AudioManager.AUDIOFOCUS_GAIN:
                publish(true);
                break;
            case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT:
            case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK:
                publish(false);
                break;
            case AudioManager.AUDIOFOCUS_LOSS:
                retireRequest();
                break;
            default:
                break;
        }
    }

    /** Invalidates callbacks before releasing permission and abandoning the exact Android request. */
    void retireRequest() {
        ++Generation;
        AudioFocusRequest retired = Request;
        Request = null;
        publish(false);
        if (retired != null) Manager.abandonAudioFocusRequest(retired);
    }

    /** Emits changes once, keeping repeated lifecycle callbacks idempotent. */
    void publish(boolean granted) {
        if (Granted == granted) return;
        Granted = granted;
        Observer.onAudioFocusChanged(granted);
    }
}
