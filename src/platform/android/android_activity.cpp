#include <android/log.h>
#include <android/native_activity.h>
#include <cstddef>

namespace {
    /// Logs lifecycle changes until the engine runtime owns the activity.
    constexpr const char* LogTag = "HelengineAndroid";

    /// Records activity teardown without retaining activity or window pointers.
    void on_destroy(ANativeActivity*) {
        __android_log_print(ANDROID_LOG_INFO, LogTag, "Native host destroyed.");
    }

    /// Marks where the future renderer will attach to a native window.
    void on_window_created(ANativeActivity*, ANativeWindow*) {
        __android_log_print(ANDROID_LOG_INFO, LogTag, "Native window ready; rendering is pending.");
    }

    /// Marks where the future renderer will release its presentation surface.
    void on_window_destroyed(ANativeActivity*, ANativeWindow*) {
        __android_log_print(ANDROID_LOG_INFO, LogTag, "Native window released.");
    }
}

/// Initializes the NativeActivity callback boundary; this scaffold does not load authored game content.
extern "C" JNIEXPORT void ANativeActivity_onCreate(ANativeActivity* activity, void*, std::size_t) {
    activity->callbacks->onDestroy = on_destroy;
    activity->callbacks->onNativeWindowCreated = on_window_created;
    activity->callbacks->onNativeWindowDestroyed = on_window_destroyed;
    __android_log_print(ANDROID_LOG_INFO, LogTag, "Android native host scaffold started.");
}
