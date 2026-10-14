#pragma once
#include <android/native_activity.h>
#include <android/looper.h>
#include <atomic>
#include <mutex>
#include <vector>

namespace helengine::android {
    /// Queues Java memory notifications for the owning native loop, including while rendering is suspended.
    class AndroidMemoryTrimBridge final {
    public:
        /// Registers a live activity and retains its prepared native looper until callback retirement.
        AndroidMemoryTrimBridge(ANativeActivity* activity,ALooper* looper);
        /// Removes the callback target under the registry lock before releasing its looper reference.
        ~AndroidMemoryTrimBridge();
        /// Prevents copying the live callback target and looper reference.
        AndroidMemoryTrimBridge(const AndroidMemoryTrimBridge&)=delete;
        /// Prevents assigning another callback target's lifetime.
        AndroidMemoryTrimBridge& operator=(const AndroidMemoryTrimBridge&)=delete;
        /// Atomically coalesces severity and wakes a suspended engine loop without allocating or touching Vulkan.
        void Request(int level);
        /// Transfers pending severity to the host thread; zero means no queued notification.
        int TakePendingLevel() { return PendingLevel.exchange(0,std::memory_order_acquire); }
        /// Routes a UI-thread JNI notification only to its still-live activity registration.
        static void NotifyActivity(JNIEnv* environment,jobject activity,int level);
    private:
        /// Historical RUNNING_LOW ordinal also accepts UI_HIDDEN, BACKGROUND and future stronger notifications.
        static constexpr int MinimumTrimLevel=10;
        /// Matches the Java callback against the NativeActivity's retained object reference.
        ANativeActivity* Activity;
        /// Retained engine looper is woken when an inactive host has cleanup to perform.
        ALooper* Looper;
        /// Strongest notification not yet consumed by the owning native loop.
        std::atomic<int> PendingLevel{0};
        /// Serializes registration, callback lookup and destruction.
        static std::mutex RegistryMutex;
        /// Contains only live callback targets; the diagnostic probe has none because it has no frame arenas.
        static std::vector<AndroidMemoryTrimBridge*> Registry;
    };
}
