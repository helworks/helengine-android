#pragma once

#include <vulkan/vulkan.h>
#include <stdexcept>
#include <string>

namespace helengine::android {
    /// Converts an unsuccessful Vulkan operation into an explicit native-host failure.
    inline void check_vulkan(VkResult result, const char* operation) {
        if (result != VK_SUCCESS) {
            throw std::runtime_error(std::string(operation) + " failed with VkResult " + std::to_string(result));
        }
    }
}
