#include "AndroidAudioFocus.hpp"
#include <android/log.h>
#include <algorithm>
#include <stdexcept>

namespace helengine::android {
    std::mutex AndroidAudioFocus::RegistryMutex;
    std::vector<AndroidAudioFocus*> AndroidAudioFocus::Registry;

    AndroidAudioFocus::AndroidAudioFocus(ANativeActivity* activity,AndroidAudioBackend& audio) : Activity(activity),Audio(audio) {
        if (!Activity || !Activity->vm || !Activity->clazz || Activity->vm->GetEnv(reinterpret_cast<void**>(&Environment),JNI_VERSION_1_6)!=JNI_OK) {
            throw std::runtime_error("Android audio focus requires a live activity and attached host thread.");
        }
        const auto activityClass=Environment->GetObjectClass(Activity->clazz);
        if (activityClass) {
            SetActiveMethod=Environment->GetMethodID(activityClass,"setGameAudioFocusActive","(Z)V");
            Environment->DeleteLocalRef(activityClass);
        }
        if (Environment->ExceptionCheck() || !SetActiveMethod) {
            Environment->ExceptionClear();
            throw std::runtime_error("Android activity is missing the native audio-focus bridge.");
        }
        Audio.SetFocusPaused(true);
        std::lock_guard<std::mutex> lock(RegistryMutex);
        Registry.push_back(this);
    }
    AndroidAudioFocus::~AndroidAudioFocus() {
        // Cleanup cannot propagate an exception across native activity destruction.
        try { SetActive(false); }
        catch (const std::exception& error) { __android_log_print(ANDROID_LOG_ERROR,"HelengineAndroid","Audio focus teardown failed: %s",error.what()); }
        std::lock_guard<std::mutex> lock(RegistryMutex);
        Registry.erase(std::remove(Registry.begin(),Registry.end(),this),Registry.end());
    }
    void AndroidAudioFocus::SetActive(bool active) {
        Environment->CallVoidMethod(Activity->clazz,SetActiveMethod,active?JNI_TRUE:JNI_FALSE);
        if (Environment->ExceptionCheck()) {
            Environment->ExceptionClear();
            throw std::runtime_error("Android audio-focus request failed with a Java exception.");
        }
    }
    void AndroidAudioFocus::NotifyActivity(JNIEnv* environment,jobject activity,bool granted) {
        std::lock_guard<std::mutex> lock(RegistryMutex);
        for (auto* registration:Registry) {
            if (environment->IsSameObject(registration->Activity->clazz,activity)) {
                registration->Audio.SetFocusPaused(!granted);
                break;
            }
        }
    }
}

/// Handles UI-thread focus callbacks without calling AAudio lifecycle functions or retaining a Java-owned native pointer.
extern "C" JNIEXPORT void JNICALL Java_com_helengine_android_HelengineActivity_nativeSetAudioFocusGranted(JNIEnv* environment,jobject activity,jboolean granted) {
    try { helengine::android::AndroidAudioFocus::NotifyActivity(environment,activity,granted==JNI_TRUE); }
    catch (const std::exception& error) {
        const auto errorClass=environment->FindClass("java/lang/IllegalStateException");
        if (errorClass) { environment->ThrowNew(errorClass,error.what()); environment->DeleteLocalRef(errorClass); }
    }
}
