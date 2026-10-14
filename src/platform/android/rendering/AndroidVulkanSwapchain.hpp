#pragma once

#include "AndroidVulkanDevice.hpp"
#include <android/native_window.h>
#include <vector>

namespace helengine::android {
    /// Owns the surface-sized presentation images, render pass, framebuffers, and per-image presentation semaphores.
    class AndroidVulkanSwapchain {
    public:
        /// Binds swapchain resource ownership to the renderer's longer-lived device.
        explicit AndroidVulkanSwapchain(AndroidVulkanDevice& device);
        /// Releases any remaining swapchain resources before device destruction.
        ~AndroidVulkanSwapchain();
        /// Prevents copying swapchain resource ownership.
        AndroidVulkanSwapchain(const AndroidVulkanSwapchain&) = delete;
        /// Prevents assigning swapchain resource ownership.
        AndroidVulkanSwapchain& operator=(const AndroidVulkanSwapchain&) = delete;
        /// Creates a FIFO swapchain and compatible render targets for a live Android surface.
        void Initialize(VkSurfaceKHR surface, ANativeWindow* window, VkExtent2D contentExtent);
        /// Releases surface-sized resources after the owning renderer has waited for GPU work.
        void Reset();
        /// Gets the presentation swapchain used for image acquisition and presentation.
        VkSwapchainKHR GetSwapchain() const { return Swapchain; }
        /// Gets the clear/store render pass used by the native Vulkan probe.
        VkRenderPass GetRenderPass() const { return RenderPass; }
        /// Gets the compatible pass that preserves native-resolution content between world composites and overlays.
        VkRenderPass GetLoadRenderPass() const { return LoadRenderPass; }
        /// Gets the exact attachment format used by compatible sampled world targets.
        VkFormat GetFormat() const { return Format; }
        /// Gets the framebuffer for the acquired image.
        VkFramebuffer GetFramebuffer(std::uint32_t image) const { return Framebuffers.at(image); }
        /// Gets the presentation signal associated with this swapchain image, rather than an in-flight frame.
        VkSemaphore GetRenderFinished(std::uint32_t image) const { return RenderFinished.at(image); }
        /// Gets the surface-sized render area and static pipeline viewport extent.
        VkExtent2D GetExtent() const { return Extent; }
        /// Gets whether the selected surface format performs sRGB encoding of shader output.
        bool IsSrgb() const { return Format == VK_FORMAT_B8G8R8A8_SRGB || Format == VK_FORMAT_R8G8B8A8_SRGB; }

    private:
        /// Device that remains alive until all swapchain resources have been released.
        AndroidVulkanDevice& Device;
        /// Presentation swapchain belonging to the attached Android surface.
        VkSwapchainKHR Swapchain = VK_NULL_HANDLE;
        /// Render pass clearing the acquired image and transitioning it for presentation.
        VkRenderPass RenderPass = VK_NULL_HANDLE;
        /// Continuation pass loads prior presentation color instead of clearing it.
        VkRenderPass LoadRenderPass = VK_NULL_HANDLE;
        /// Selected RGBA/BGRA surface pixel format.
        VkFormat Format = VK_FORMAT_UNDEFINED;
        /// Current swapchain image dimensions.
        VkExtent2D Extent{};
        /// Views selecting the color attachment in each swapchain image.
        std::vector<VkImageView> ImageViews;
        /// Render targets pairing those image views with the current extent and render pass.
        std::vector<VkFramebuffer> Framebuffers;
        /// Per-image semaphores safely reused after that image is reacquired.
        std::vector<VkSemaphore> RenderFinished;
        /// Serialized depth target shared by framebuffers; the host allows only one frame in flight.
        VkImage DepthImage = VK_NULL_HANDLE;
        /// Memory backing the surface-sized depth image.
        VkDeviceMemory DepthMemory = VK_NULL_HANDLE;
        /// View bound to the depth attachment of game framebuffers.
        VkImageView DepthView = VK_NULL_HANDLE;
    };
}
