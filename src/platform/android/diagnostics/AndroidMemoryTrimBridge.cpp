#include "AndroidMemoryTrimBridge.hpp"
#include <algorithm>
#include <stdexcept>

namespace helengine::android {
    std::mutex AndroidMemoryTrimBridge::RegistryMutex;
    std::vector<AndroidMemoryTrimBridge*> AndroidMemoryTrimBridge::Registry;
    AndroidMemoryTrimBridge::AndroidMemoryTrimBridge(ANativeActivity* activity,ALooper* looper) : Activity(activity),Looper(looper) {
        if (!Activity || !Activity->clazz || !Looper) throw std::invalid_argument("Android memory trim requires a live activity and prepared host looper.");
        ALooper_acquire(Looper);
        try { std::lock_guard<std::mutex> lock(RegistryMutex); Registry.push_back(this); }
        catch (...) { ALooper_release(Looper); throw; }
    }
    AndroidMemoryTrimBridge::~AndroidMemoryTrimBridge() {
        std::lock_guard<std::mutex> lock(RegistryMutex);
        Registry.erase(std::remove(Registry.begin(),Registry.end(),this),Registry.end());
        ALooper_release(Looper);
    }
    void AndroidMemoryTrimBridge::Request(int level) {
        if (level<MinimumTrimLevel) return;
        int previous=PendingLevel.load(std::memory_order_relaxed);
        do { if (previous>=level) return; }
        while (!PendingLevel.compare_exchange_weak(previous,level,std::memory_order_release,std::memory_order_relaxed));
        ALooper_wake(Looper);
    }
    void AndroidMemoryTrimBridge::NotifyActivity(JNIEnv* environment,jobject activity,int level) {
        std::lock_guard<std::mutex> lock(RegistryMutex);
        for (auto* registration:Registry) {
            if (environment->IsSameObject(registration->Activity->clazz,activity)) { registration->Request(level); break; }
        }
    }
}

/// Handles notifications for either APK flavor; no registration means there are no game frame caches to release.
extern "C" JNIEXPORT void JNICALL Java_com_helengine_android_HelengineActivity_nativeRequestMemoryTrim(JNIEnv* environment,jobject activity,jint level) {
    try { helengine::android::AndroidMemoryTrimBridge::NotifyActivity(environment,activity,level); }
    catch (const std::exception& error) {
        const auto errorClass=environment->FindClass("java/lang/IllegalStateException");
        if (errorClass) { environment->ThrowNew(errorClass,error.what()); environment->DeleteLocalRef(errorClass); }
    }
}
