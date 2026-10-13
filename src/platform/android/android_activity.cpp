#include "rendering/AndroidVulkanRenderer.hpp"

#include <android/log.h>
#include <android/native_activity.h>
#include <cstddef>
#include <exception>

namespace {
    /// Logs lifecycle changes until the engine runtime owns the activity.
    constexpr const char* LogTag = "HelengineAndroid";

    /// Retrieves the renderer explicitly owned by the native activity.
    helengine::android::AndroidVulkanRenderer& get_renderer(ANativeActivity* activity) {
        if (activity->instance == nullptr) throw std::logic_error("Android Vulkan renderer was not initialized.");
        return *static_cast<helengine::android::AndroidVulkanRenderer*>(activity->instance);
    }

    /// Records a native renderer failure and finishes the activity instead of hiding it behind another backend.
    void report_failure(ANativeActivity* activity, const char* operation, const std::exception& error) {
        __android_log_print(ANDROID_LOG_ERROR, LogTag, "%s failed: %s", operation, error.what());
        ANativeActivity_finish(activity);
    }

    /// Releases activity-owned Vulkan resources after its native window has been detached.
    void on_destroy(ANativeActivity* activity) {
        delete static_cast<helengine::android::AndroidVulkanRenderer*>(activity->instance);
        activity->instance = nullptr;
        __android_log_print(ANDROID_LOG_INFO, LogTag, "Native host destroyed.");
    }

    /// Presents a Vulkan frame synchronously when Android requests a surface redraw.
    void on_window_redraw(ANativeActivity* activity, ANativeWindow* window) {
        (void)window;
        try {
            get_renderer(activity).DrawFrame();
        } catch (const std::exception& error) {
            report_failure(activity, "Vulkan redraw", error);
        }
    }

    /// Recreates extent-dependent resources before repainting a resized Vulkan surface.
    void on_window_resized(ANativeActivity* activity, ANativeWindow* window) {
        (void)window;
        try {
            get_renderer(activity).Resize();
        } catch (const std::exception& error) {
            report_failure(activity, "Vulkan resize", error);
        }
    }

    /// Forwards the actual content-layout dimensions when Android resizes the activity's native drawing area.
    void on_content_rect_changed(ANativeActivity* activity, const ARect* rect) {
        try {
            const int width = rect->right - rect->left;
            const int height = rect->bottom - rect->top;
            if (width <= 0 || height <= 0) throw std::invalid_argument("Android content rectangle is not drawable.");
            get_renderer(activity).SetContentExtent(static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height));
        } catch (const std::exception& error) {
            report_failure(activity, "Vulkan content layout", error);
        }
    }

    /// Attaches the new Android surface and draws the first GPU-rendered probe frame.
    void on_window_created(ANativeActivity* activity, ANativeWindow* window) {
        try {
            get_renderer(activity).AttachWindow(window);
            __android_log_print(ANDROID_LOG_INFO, LogTag, "Native window ready.");
            get_renderer(activity).DrawFrame();
        } catch (const std::exception& error) {
            report_failure(activity, "Vulkan surface initialization", error);
        }
    }

    /// Releases Vulkan resources before Android invalidates the borrowed native window.
    void on_window_destroyed(ANativeActivity* activity, ANativeWindow* window) {
        (void)window;
        try {
            get_renderer(activity).DetachWindow();
        } catch (const std::exception& error) {
            report_failure(activity, "Vulkan surface teardown", error);
        }
        __android_log_print(ANDROID_LOG_INFO, LogTag, "Native window released.");
    }
}

/// Initializes the NativeActivity callback boundary; this scaffold does not load authored game content.
extern "C" JNIEXPORT void ANativeActivity_onCreate(ANativeActivity* activity, void*, std::size_t) {
    activity->callbacks->onDestroy = on_destroy;
    activity->callbacks->onNativeWindowCreated = on_window_created;
    activity->callbacks->onNativeWindowResized = on_window_resized;
    activity->callbacks->onNativeWindowRedrawNeeded = on_window_redraw;
    activity->callbacks->onNativeWindowDestroyed = on_window_destroyed;
    activity->callbacks->onContentRectChanged = on_content_rect_changed;
    try {
        activity->instance = new helengine::android::AndroidVulkanRenderer(activity->assetManager);
        __android_log_print(ANDROID_LOG_INFO, LogTag, "Android native host scaffold started: Vulkan.");
    } catch (const std::exception& error) {
        report_failure(activity, "Vulkan renderer initialization", error);
    }
}
