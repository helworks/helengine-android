#pragma once

#include <android/native_window.h>

namespace helengine::android {
    /// Presents an opaque cornflower-blue boot frame through the native window's CPU drawing buffer.
    /// Returns the Android window API result: zero on success or a negative error code.
    int present_boot_frame(ANativeWindow* window);
}
