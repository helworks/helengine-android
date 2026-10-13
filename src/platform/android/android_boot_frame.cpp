#include "android_boot_frame.hpp"

#include <cstddef>
#include <cstdint>

namespace helengine::android {
    int present_boot_frame(ANativeWindow* window) {
        const int geometry_result = ANativeWindow_setBuffersGeometry(window, 0, 0, WINDOW_FORMAT_RGBA_8888);
        if (geometry_result != 0) {
            return geometry_result;
        }

        ANativeWindow_Buffer buffer{};
        const int lock_result = ANativeWindow_lock(window, &buffer, nullptr);
        if (lock_result != 0) {
            return lock_result;
        }

        auto* pixels = static_cast<std::uint8_t*>(buffer.bits);
        for (int y = 0; y < buffer.height; ++y) {
            const std::size_t row_offset = static_cast<std::size_t>(y) * buffer.stride * 4;
            for (int x = 0; x < buffer.width; ++x) {
                const std::size_t pixel_offset = row_offset + static_cast<std::size_t>(x) * 4;
                pixels[pixel_offset] = 100;
                pixels[pixel_offset + 1] = 149;
                pixels[pixel_offset + 2] = 237;
                pixels[pixel_offset + 3] = 255;
            }
        }

        return ANativeWindow_unlockAndPost(window);
    }
}
