#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>

namespace helengine::android {
    /// Owns the Vulkan instance, graphics/presentation queue, and serialized frame submission resources.
    class AndroidVulkanDevice {
    public:
        /// Creates the Android-capable Vulkan instance without binding it to a window.
        AndroidVulkanDevice();
        /// Releases submission resources, the logical device, and the Vulkan instance.
        ~AndroidVulkanDevice();
        /// Prevents copying native resource ownership.
        AndroidVulkanDevice(const AndroidVulkanDevice&) = delete;
        /// Prevents assigning native resource ownership.
        AndroidVulkanDevice& operator=(const AndroidVulkanDevice&) = delete;
        /// Selects a device and a queue that can both render and present to the supplied surface.
        void Initialize(VkSurfaceKHR surface);
        /// Waits for device work before releasing a surface or its rendering resources.
        void WaitIdle() const;
        /// Gets whether the logical device has been selected for a presentation surface.
        bool IsInitialized() const { return Device != VK_NULL_HANDLE; }
        /// Gets the Android-enabled Vulkan instance.
        VkInstance GetInstance() const { return Instance; }
        /// Gets the selected physical device for surface capability queries.
        VkPhysicalDevice GetPhysicalDevice() const { return PhysicalDevice; }
        /// Gets the logical device used by all renderer resources.
        VkDevice GetDevice() const { return Device; }
        /// Gets the graphics/presentation queue.
        VkQueue GetQueue() const { return Queue; }
        /// Gets the queue family used for exclusive swapchain image ownership.
        std::uint32_t GetQueueFamily() const { return QueueFamily; }
        /// Gets the reusable command buffer protected by the frame fence.
        VkCommandBuffer GetCommandBuffer() const { return CommandBuffer; }
        /// Gets the acquire semaphore reused only after the preceding submission has completed.
        VkSemaphore GetImageAvailable() const { return ImageAvailable; }
        /// Gets the fence protecting command buffer and acquire-semaphore reuse.
        VkFence GetFrameFence() const { return FrameFence; }

    private:
        /// Instance with the Android surface extensions enabled.
        VkInstance Instance = VK_NULL_HANDLE;
        /// Device whose queue supports graphics and presentation to the current surface.
        VkPhysicalDevice PhysicalDevice = VK_NULL_HANDLE;
        /// Logical device with the swapchain extension enabled.
        VkDevice Device = VK_NULL_HANDLE;
        /// Queue used to submit rendering and presentation operations.
        VkQueue Queue = VK_NULL_HANDLE;
        /// Queue family index chosen during device selection.
        std::uint32_t QueueFamily = 0;
        /// Pool permitting the serialized frame command buffer to be reset.
        VkCommandPool CommandPool = VK_NULL_HANDLE;
        /// Command buffer reused after its submission fence is signaled.
        VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;
        /// Image acquisition signal consumed by the graphics submission.
        VkSemaphore ImageAvailable = VK_NULL_HANDLE;
        /// Initially signaled fence guarding the single frame in flight.
        VkFence FrameFence = VK_NULL_HANDLE;
    };
}
