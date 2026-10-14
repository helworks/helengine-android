package com.helengine.android;
import android.app.Activity;
import android.media.AudioAttributes;
import android.media.AudioFocusRequest;
import android.media.AudioManager;
import android.os.Build;
import android.os.Looper;
import android.os.Bundle;

/** Runs the production focus controller against deterministic Android-shaped service callbacks on a desktop JVM. */
public final class AndroidAudioFocusTests {
    /** Executes independent permission, lifetime and API compatibility scenarios. */
    public static void main(String[] args) throws InterruptedException {
        grantsOnceAndAbandonsExactRequest();
        transientLossAndDuckResumeOnlyCurrentRequest();
        permanentLossRequiresNewActivation();
        delayedAndDeniedRequestsRemainSilent();
        staleCallbacksCannotOverrideAReplacement();
        synchronousCallbacksWinOverRequestReturn();
        attributesMatchLegacyAndModernOutput();
        suspensionWinsOverAConcurrentNativeResume();
        System.out.println("Passed 8 Android audio focus scenarios.");
    }
    /** Throws a descriptive test failure without external dependencies. */
    static void require(boolean condition,String message) { if (!condition) throw new AssertionError(message); }
    /** Activations are idempotent and retirement rejects a gain callback even during abandonment. */
    static void grantsOnceAndAbandonsExactRequest() {
        Activity activity = new Activity(); FocusRecorder observer = new FocusRecorder();
        AndroidAudioFocus focus = new AndroidAudioFocus(activity,observer);
        focus.setActive(false);
        require(observer.changes.isEmpty(),"Inactive diagnostic host called a native audio bridge.");
        focus.setActive(true); focus.setActive(true);
        AudioFocusRequest request = activity.manager.requests.get(0);
        require(observer.granted && activity.manager.requests.size()==1,"Repeated activation requested duplicate focus.");
        activity.manager.gainDuringAbandon = true;
        focus.setActive(false); focus.setActive(false);
        require(!observer.granted && observer.changes.size()==2,"Abandonment callback reactivated suspended audio.");
        require(activity.manager.abandoned.size()==1 && activity.manager.abandoned.get(0)==request,"Focus was not abandoned with its exact request.");
    }
    /** Both temporary loss kinds freeze playback and a current gain may resume it. */
    static void transientLossAndDuckResumeOnlyCurrentRequest() {
        Activity activity = new Activity(); FocusRecorder observer = new FocusRecorder();
        AndroidAudioFocus focus = new AndroidAudioFocus(activity,observer); focus.setActive(true);
        AudioFocusRequest request = activity.manager.requests.get(0);
        request.listener.onAudioFocusChange(AudioManager.AUDIOFOCUS_LOSS_TRANSIENT);
        require(!observer.granted,"Transient loss did not revoke audio permission.");
        request.listener.onAudioFocusChange(AudioManager.AUDIOFOCUS_GAIN);
        require(observer.granted,"Current transient gain did not permit playback.");
        request.listener.onAudioFocusChange(AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK);
        require(!observer.granted,"Duck loss did not apply the requested pause policy.");
        request.listener.onAudioFocusChange(AudioManager.AUDIOFOCUS_GAIN);
        require(observer.granted && activity.manager.requests.size()==1,"Transient gain unnecessarily acquired a new request.");
    }
    /** A permanent loss does not automatically steal focus back from another app. */
    static void permanentLossRequiresNewActivation() {
        Activity activity = new Activity(); FocusRecorder observer = new FocusRecorder();
        AndroidAudioFocus focus = new AndroidAudioFocus(activity,observer); focus.setActive(true);
        AudioFocusRequest previous = activity.manager.requests.get(0);
        previous.listener.onAudioFocusChange(AudioManager.AUDIOFOCUS_LOSS);
        previous.listener.onAudioFocusChange(AudioManager.AUDIOFOCUS_GAIN); focus.setActive(true);
        require(!observer.granted && activity.manager.requests.size()==1,"Permanent loss automatically reacquired focus.");
        focus.setActive(false); focus.setActive(true);
        require(observer.granted && activity.manager.requests.size()==2,"New lifecycle activation did not request fresh focus.");
    }
    /** Delayed focus requires a valid callback; denied requests ignore any late gain. */
    static void delayedAndDeniedRequestsRemainSilent() {
        Activity activity = new Activity(); FocusRecorder observer = new FocusRecorder();
        activity.manager.result = AudioManager.AUDIOFOCUS_REQUEST_DELAYED;
        AndroidAudioFocus focus = new AndroidAudioFocus(activity,observer); focus.setActive(true);
        require(!observer.granted,"Delayed request started audio before permission.");
        activity.manager.requests.get(0).listener.onAudioFocusChange(AudioManager.AUDIOFOCUS_GAIN);
        require(observer.granted,"Delayed gain was ignored.");
        focus.setActive(false); activity.manager.result = AudioManager.AUDIOFOCUS_REQUEST_FAILED; focus.setActive(true);
        activity.manager.requests.get(1).listener.onAudioFocusChange(AudioManager.AUDIOFOCUS_GAIN);
        require(!observer.granted,"Denied request accepted a stale gain callback.");
    }
    /** Old requests cannot mutate permission after a new, still-delayed request replaces them. */
    static void staleCallbacksCannotOverrideAReplacement() {
        Activity activity = new Activity(); FocusRecorder observer = new FocusRecorder();
        AndroidAudioFocus focus = new AndroidAudioFocus(activity,observer); focus.setActive(true);
        AudioFocusRequest previous = activity.manager.requests.get(0);
        focus.setActive(false); activity.manager.result = AudioManager.AUDIOFOCUS_REQUEST_DELAYED; focus.setActive(true);
        previous.listener.onAudioFocusChange(AudioManager.AUDIOFOCUS_GAIN);
        require(!observer.granted,"Previous request granted permission to its replacement.");
        AudioFocusRequest current = activity.manager.requests.get(1);
        current.listener.onAudioFocusChange(AudioManager.AUDIOFOCUS_GAIN);
        previous.listener.onAudioFocusChange(AudioManager.AUDIOFOCUS_LOSS);
        require(observer.granted,"Previous loss revoked the new request's permission.");
    }
    /** A reentrant loss or gain cannot be overwritten by the return value of requestAudioFocus. */
    static void synchronousCallbacksWinOverRequestReturn() {
        Activity activity = new Activity(); FocusRecorder observer = new FocusRecorder();
        activity.manager.synchronousChange = AudioManager.AUDIOFOCUS_LOSS_TRANSIENT;
        AndroidAudioFocus focus = new AndroidAudioFocus(activity,observer); focus.setActive(true);
        require(!observer.granted,"Request return overwrote a synchronous transient loss.");
        focus.setActive(false); activity.manager.result = AudioManager.AUDIOFOCUS_REQUEST_DELAYED;
        activity.manager.synchronousChange = AudioManager.AUDIOFOCUS_GAIN; focus.setActive(true);
        require(observer.granted,"Request return overwrote a synchronous gain.");
    }
    /** Focus attributes and callback policy match native client capabilities on both supported API paths. */
    static void attributesMatchLegacyAndModernOutput() {
        for (int api : new int[] {26,27,28,35}) {
            Build.VERSION.SDK_INT = api;
            Activity activity = new Activity();
            AndroidAudioFocus focus = new AndroidAudioFocus(activity,new FocusRecorder()); focus.setActive(true);
            AudioFocusRequest request = activity.manager.requests.get(0);
            require(request.attributes.usage==(api>=28 ? AudioAttributes.USAGE_GAME : AudioAttributes.USAGE_MEDIA),"Focus usage disagrees with the native client.");
            require(request.attributes.contentType==AudioAttributes.CONTENT_TYPE_MUSIC,"Focus content type did not describe PCM playback.");
            require(request.delayed && request.pauseWhenDucked && request.handler.looper==Looper.getMainLooper(),"Focus request omitted callback or interruption policy.");
            focus.setActive(false);
        }
        Build.VERSION.SDK_INT = 35;
    }
    /** Pausing under the controller lock prevents an already-blocked native request from publishing a stale resume. */
    static void suspensionWinsOverAConcurrentNativeResume() throws InterruptedException {
        HelengineActivity activity = new HelengineActivity();
        activity.onCreate(new Bundle());
        FocusRecorder observer = new FocusRecorder();
        activity.AudioFocus = new AndroidAudioFocus(activity,observer);
        activity.onResume();
        Thread requester = new Thread(() -> activity.setGameAudioFocusActive(true));
        synchronized (activity.AudioFocus) {
            requester.start();
            long deadline = System.nanoTime() + 1_000_000_000L;
            while (requester.getState() != Thread.State.BLOCKED && System.nanoTime() < deadline) Thread.sleep(1);
            require(requester.getState()==Thread.State.BLOCKED,"Native activation never reached the focus lock.");
            activity.onPause();
        }
        requester.join(1000);
        require(!requester.isAlive(),"Native activation remained blocked after suspension.");
        require(!observer.granted && activity.manager.requests.isEmpty(),"Concurrent native resume acquired focus after UI suspension.");
        activity.onDestroy();
    }
}
