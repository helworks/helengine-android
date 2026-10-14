#include "AndroidVulkanRenderer.hpp"
#include "vulkan_check.hpp"

#include <android/log.h>
#include <cmath>
#include <limits>

namespace helengine::android {
    AndroidVulkanRenderer::AndroidVulkanRenderer(AAssetManager* assets)
        : Assets(assets), Device(), Swapchain(Device), Pipeline(Device), SceneGpu(Device) {
        if (Assets == nullptr) throw std::invalid_argument("Android asset manager is required.");
    }

    AndroidVulkanRenderer::~AndroidVulkanRenderer() {
        // Destruction cannot propagate a device-loss exception across the native activity boundary.
        if (Device.IsInitialized()) vkDeviceWaitIdle(Device.GetDevice());
        Pipeline.Reset();
        SceneGpu.ResetPipelines();
        Swapchain.Reset();
        if (Surface != VK_NULL_HANDLE) vkDestroySurfaceKHR(Device.GetInstance(), Surface, nullptr);
    }

    void AndroidVulkanRenderer::AttachWindow(ANativeWindow* window) {
        if (window == nullptr) throw std::invalid_argument("Android native window is required.");
        if (Surface != VK_NULL_HANDLE) throw std::logic_error("Detach the previous native window before attaching another.");
        Window = window;
        VkAndroidSurfaceCreateInfoKHR surface{};
        surface.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
        surface.window = Window;
        check_vulkan(vkCreateAndroidSurfaceKHR(Device.GetInstance(), &surface, nullptr, &Surface), "vkCreateAndroidSurfaceKHR");
        if (!Device.IsInitialized()) Device.Initialize(Surface);
        VkBool32 supported = VK_FALSE;
        check_vulkan(vkGetPhysicalDeviceSurfaceSupportKHR(Device.GetPhysicalDevice(), Device.GetQueueFamily(), Surface, &supported), "vkGetPhysicalDeviceSurfaceSupportKHR");
        if (!supported) throw std::runtime_error("Selected Vulkan queue cannot present to the replacement Android surface.");
        Swapchain.Initialize(Surface, Window, ContentExtent);
#if HELENGINE_ANDROID_GAME
        SceneGpu.Initialize(Assets, Swapchain);
#else
        Pipeline.Initialize(Assets, Swapchain);
#endif
        WindowReady = true;
    }

    void AndroidVulkanRenderer::DetachWindow() {
        WindowReady = false;
        Device.WaitIdle();
        Pipeline.Reset();
        SceneGpu.ResetPipelines();
        Swapchain.Reset();
        if (Surface != VK_NULL_HANDLE) vkDestroySurfaceKHR(Device.GetInstance(), Surface, nullptr);
        Surface = VK_NULL_HANDLE;
        Window = nullptr;
        __android_log_print(ANDROID_LOG_INFO, "HelengineAndroid", "Vulkan surface released.");
    }

    void AndroidVulkanRenderer::RecreateSwapchain() {
        WindowReady = false;
        Device.WaitIdle();
        Pipeline.Reset();
        SceneGpu.ResetPipelines();
        Swapchain.Reset();
        Swapchain.Initialize(Surface, Window, ContentExtent);
#if HELENGINE_ANDROID_GAME
        SceneGpu.Initialize(Assets, Swapchain);
#else
        Pipeline.Initialize(Assets, Swapchain);
#endif
        WindowReady = true;
    }

    void AndroidVulkanRenderer::Resize() {
        if (Surface == VK_NULL_HANDLE) throw std::logic_error("Cannot resize Vulkan without an attached native window.");
        RecreateSwapchain();
#if !HELENGINE_ANDROID_GAME
        DrawFrame();
#endif
    }

    void AndroidVulkanRenderer::SetContentExtent(std::uint32_t width, std::uint32_t height) {
        if (width == 0 || height == 0) throw std::invalid_argument("Android content extent must be drawable.");
        if (ContentExtent.width == width && ContentExtent.height == height) return;
        ContentExtent = {width, height};
        __android_log_print(ANDROID_LOG_INFO, "HelengineAndroid", "Vulkan content extent changed: %ux%u.", width, height);
        if (Surface != VK_NULL_HANDLE) Resize();
    }

    float AndroidVulkanRenderer::ToLinearColor(std::uint8_t channel) {
        const double color = static_cast<double>(channel) / 255.0;
        return static_cast<float>(color <= 0.04045 ? color / 12.92 : std::pow((color + 0.055) / 1.055, 2.4));
    }

    void AndroidVulkanRenderer::DrawFrame() {
        if (!WindowReady) throw std::logic_error("Cannot draw Vulkan before all native-window resources are initialized.");
        const VkDevice device = Device.GetDevice();
        const VkFence fence = Device.GetFrameFence();
        const auto timeout = std::numeric_limits<std::uint64_t>::max();
        check_vulkan(vkWaitForFences(device, 1, &fence, VK_TRUE, timeout), "vkWaitForFences");
#if HELENGINE_ANDROID_GAME
        SceneGpu.Prepare(SceneBatches);
#endif
        std::uint32_t image = 0;
        VkResult acquisition = vkAcquireNextImageKHR(device, Swapchain.GetSwapchain(), timeout, Device.GetImageAvailable(), VK_NULL_HANDLE, &image);
        if (acquisition == VK_ERROR_OUT_OF_DATE_KHR) {
            RecreateSwapchain();
            acquisition = vkAcquireNextImageKHR(device, Swapchain.GetSwapchain(), timeout, Device.GetImageAvailable(), VK_NULL_HANDLE, &image);
        }
        if (acquisition != VK_SUBOPTIMAL_KHR) check_vulkan(acquisition, "vkAcquireNextImageKHR");
        const VkCommandBuffer command = Device.GetCommandBuffer();
        check_vulkan(vkResetCommandBuffer(command, 0), "vkResetCommandBuffer");
        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        check_vulkan(vkBeginCommandBuffer(command, &begin), "vkBeginCommandBuffer");
#if HELENGINE_ANDROID_GAME
        SceneGpu.RecordShadows(command,SceneBatches);
#endif
        VkClearValue clear{};
        clear.color.float32[0] = Swapchain.IsSrgb() ? ToLinearColor(100) : 100.0f / 255.0f;
        clear.color.float32[1] = Swapchain.IsSrgb() ? ToLinearColor(149) : 149.0f / 255.0f;
        clear.color.float32[2] = Swapchain.IsSrgb() ? ToLinearColor(237) : 237.0f / 255.0f;
        clear.color.float32[3] = 1.0f;
        VkRenderPassBeginInfo render{};
        render.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render.renderPass = Swapchain.GetRenderPass();
        render.framebuffer = Swapchain.GetFramebuffer(image);
        render.renderArea.extent = Swapchain.GetExtent();
        render.clearValueCount = 1;
#if HELENGINE_ANDROID_GAME
        VkClearValue clears[2]{};
        clears[0].color = ClearColor;
        clears[1].depthStencil = {1, 0};
        render.clearValueCount = 2;
        render.pClearValues = clears;
#else
        render.pClearValues = &clear;
#endif
        vkCmdBeginRenderPass(command, &render, VK_SUBPASS_CONTENTS_INLINE);
#if HELENGINE_ANDROID_GAME
        SceneGpu.Record(command, SceneBatches);
#else
        vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline.GetPipeline());
        vkCmdDraw(command, 3, 1, 0, 0);
#endif
        vkCmdEndRenderPass(command);
        check_vulkan(vkEndCommandBuffer(command), "vkEndCommandBuffer");
        const VkSemaphore acquire = Device.GetImageAvailable();
        const VkSemaphore finished = Swapchain.GetRenderFinished(image);
        const VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = &acquire;
        submit.pWaitDstStageMask = &wait_stage;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &command;
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = &finished;
        check_vulkan(vkResetFences(device, 1, &fence), "vkResetFences");
        check_vulkan(vkQueueSubmit(Device.GetQueue(), 1, &submit, fence), "vkQueueSubmit");
        const VkSwapchainKHR swapchain = Swapchain.GetSwapchain();
        VkPresentInfoKHR present{};
        present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores = &finished;
        present.swapchainCount = 1;
        present.pSwapchains = &swapchain;
        present.pImageIndices = &image;
        const VkResult result = vkQueuePresentKHR(Device.GetQueue(), &present);
        if (result != VK_ERROR_OUT_OF_DATE_KHR && result != VK_SUBOPTIMAL_KHR) check_vulkan(result, "vkQueuePresentKHR");
        // Logical-orientation presentation can remain suboptimal relative to Android's preferred pre-rotation.
        // A suboptimal swapchain is drawable; native window/content callbacks own extent changes.
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            RecreateSwapchain();
        } else {
#if HELENGINE_ANDROID_GAME
            if ((PresentedFrames++ % 120) == 0) {
                std::size_t vertices = 0;
                for (const auto& batch : SceneBatches) vertices += batch.Vertices.size();
                __android_log_print(ANDROID_LOG_INFO, "HelengineAndroid", "Vulkan scene frame presented: frame=%llu, %ux%u, batches=%zu vertices=%zu.",
                    static_cast<unsigned long long>(PresentedFrames), Swapchain.GetExtent().width, Swapchain.GetExtent().height, SceneBatches.size(), vertices);
            }
#else
            __android_log_print(ANDROID_LOG_INFO, "HelengineAndroid", "Vulkan triangle frame presented: %ux%u.", Swapchain.GetExtent().width, Swapchain.GetExtent().height);
#endif
        }
    }
}
