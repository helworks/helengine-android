#pragma once
#include "AndroidAudioBackend.hpp"
#include <android/native_activity.h>
#include <mutex>
#include <vector>

namespace helengine::android {
    /// Registers activity-owned focus callbacks while stream lifecycle work remains on the native engine thread.
    class AndroidAudioFocus {
    public:
        /// Borrows the activity and mixer, both of which outlive this registration.
        AndroidAudioFocus(ANativeActivity* activity,AndroidAudioBackend& audio);
        /// Abandons focus and unregisters the mixer before callback-visible ownership is released.
        ~AndroidAudioFocus();
        /// Prevents duplicating a registry entry or borrowed native mixer binding.
        AndroidAudioFocus(const AndroidAudioFocus&)=delete;
        /// Prevents replacing the activity associated with a registered callback.
        AndroidAudioFocus& operator=(const AndroidAudioFocus&)=delete;
        /// Requests or abandons focus through the current activity's lifecycle gate.
        void SetActive(bool active);
        /// Applies Java callback permission only to its still-live matching activity.
        static void NotifyActivity(JNIEnv* environment,jobject activity,bool granted);
    private:
        /// Activity reference remains valid until this registration is removed from the native registry.
        ANativeActivity* Activity;
        /// Backend owns the independent host-pause and focus-pause gates.
        AndroidAudioBackend& Audio;
        /// Renderer attaches the engine thread before constructing this host-owned bridge.
        JNIEnv* Environment=nullptr;
        /// Cached Java lifecycle method avoids reflection or local-reference allocation per frame.
        jmethodID SetActiveMethod=nullptr;
        /// Serializes callback dispatch with registration destruction.
        static std::mutex RegistryMutex;
        /// Borrowed registrations live only until their owner begins destruction.
        static std::vector<AndroidAudioFocus*> Registry;
    };
}
