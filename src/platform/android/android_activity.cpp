#include "android_boot_frame.hpp"

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

    /// Presents the boot frame synchronously when Android needs the surface to be redrawn.
    void on_window_redraw(ANativeActivity* activity, ANativeWindow* window) {
        const int result = helengine::android::present_boot_frame(window);
        if (result != 0) {
            __android_log_print(ANDROID_LOG_ERROR, LogTag, "Boot frame presentation failed: %d", result);
            ANativeActivity_finish(activity);
            return;
        }
        __android_log_print(ANDROID_LOG_INFO, LogTag, "Blue boot frame presented: %dx%d, RGBA 100,149,237,255.",
            ANativeWindow_getWidth(window), ANativeWindow_getHeight(window));
    }

    /// Draws a visible boot frame as soon as Android creates the native surface.
    void on_window_created(ANativeActivity* activity, ANativeWindow* window) {
        __android_log_print(ANDROID_LOG_INFO, LogTag, "Native window ready.");
        on_window_redraw(activity, window);
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
    activity->callbacks->onNativeWindowResized = on_window_redraw;
    activity->callbacks->onNativeWindowRedrawNeeded = on_window_redraw;
    activity->callbacks->onNativeWindowDestroyed = on_window_destroyed;
    __android_log_print(ANDROID_LOG_INFO, LogTag, "Android native host scaffold started.");
}
