#pragma once

#include "AndroidVulkanDevice.hpp"
#include "AndroidVulkanSwapchain.hpp"
#include "AndroidVulkanPipeline.hpp"

namespace helengine::android {
    /// Presents the native Vulkan probe while keeping device and surface ownership separate across Android lifecycle events.
    class AndroidVulkanRenderer {
    public:
        /// Creates the renderer from the activity's required packaged-asset manager.
        explicit AndroidVulkanRenderer(AAssetManager* assets);
        /// Releases any attached surface and its resources before member device destruction.
        ~AndroidVulkanRenderer();
        /// Prevents copying surface and device resource ownership.
        AndroidVulkanRenderer(const AndroidVulkanRenderer&) = delete;
        /// Prevents assigning surface and device resource ownership.
        AndroidVulkanRenderer& operator=(const AndroidVulkanRenderer&) = delete;
        /// Attaches a newly created native window and creates its Vulkan surface and presentation resources.
        void AttachWindow(ANativeWindow* window);
        /// Waits for pending device work, then releases all resources tied to the native window.
        void DetachWindow();
        /// Recreates viewport-dependent resources and draws after Android changes the window extent.
        void Resize();
        /// Applies Android's live content-layout extent instead of the dimensions retained by a producer buffer.
        void SetContentExtent(std::uint32_t width, std::uint32_t height);
        /// Draws and presents the diagnostic triangle with a single serialized frame in flight.
        void DrawFrame();

    private:
        /// Recreates the swapchain and its graphics pipeline while preserving the device and surface.
        void RecreateSwapchain();
        /// Converts a specified sRGB byte channel to the linear value required by an sRGB attachment.
        static float ToLinearColor(std::uint8_t channel);
        /// Activity-owned asset manager used only while the renderer's activity remains alive.
        AAssetManager* Assets;
        /// Device and frame resources that outlive the surface-sized resources.
        AndroidVulkanDevice Device;
        /// Surface-sized images, render targets, and presentation semaphores.
        AndroidVulkanSwapchain Swapchain;
        /// Probe graphics pipeline compatible with the current swapchain.
        AndroidVulkanPipeline Pipeline;
        /// Vulkan surface bound to the live native window.
        VkSurfaceKHR Surface = VK_NULL_HANDLE;
        /// Borrowed Android window valid until the native-window destruction callback completes.
        ANativeWindow* Window = nullptr;
        /// Latest drawable layout size reported by Android, or zero until its first content-layout callback.
        VkExtent2D ContentExtent{};
        /// Whether all resources required for drawing are initialized for the current native window.
        bool WindowReady = false;
    };
}
