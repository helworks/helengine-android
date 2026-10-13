#pragma once
#include "AndroidVulkanDevice.hpp"
#include "vulkan_check.hpp"

namespace helengine::android {
    /// Finds a memory type satisfying every required property for a Vulkan allocation.
    inline std::uint32_t FindMemoryType(const AndroidVulkanDevice& device, std::uint32_t bits, VkMemoryPropertyFlags flags) {
        VkPhysicalDeviceMemoryProperties properties{};
        vkGetPhysicalDeviceMemoryProperties(device.GetPhysicalDevice(), &properties);
        for (std::uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
            if ((bits & (1u << i)) && (properties.memoryTypes[i].propertyFlags & flags) == flags) return i;
        }
        throw std::runtime_error("The Vulkan device has no compatible memory type.");
    }

    /// Allocates and binds image memory, leaving ownership with the caller.
    inline VkDeviceMemory AllocateImageMemory(const AndroidVulkanDevice& device, VkImage image) {
        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(device.GetDevice(), image, &requirements);
        VkMemoryAllocateInfo allocation{};
        allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = FindMemoryType(device, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VkDeviceMemory memory = VK_NULL_HANDLE;
        check_vulkan(vkAllocateMemory(device.GetDevice(), &allocation, nullptr, &memory), "vkAllocateMemory(image)");
        check_vulkan(vkBindImageMemory(device.GetDevice(), image, memory, 0), "vkBindImageMemory");
        return memory;
    }
}
