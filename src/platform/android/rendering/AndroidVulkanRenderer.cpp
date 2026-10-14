#include "AndroidVulkanRenderer.hpp"
#include "../diagnostics/AndroidTraceSection.hpp"
#include "vulkan_check.hpp"
#include "SceneFramePlanner.hpp"
#include "SceneResolutionMath.hpp"

#include <android/log.h>
#include <cmath>
#include <limits>

namespace helengine::android {
    AndroidVulkanRenderer::AndroidVulkanRenderer(ANativeActivity* activity)
        : Assets(activity?activity->assetManager:nullptr), Device(), FramePacer(activity,Device), Swapchain(Device), Pipeline(Device), SceneGpu(Device) {
        if (Assets == nullptr) throw std::invalid_argument("Android asset manager is required.");
    }

    AndroidVulkanRenderer::~AndroidVulkanRenderer() {
        // Destruction cannot propagate a device-loss exception across the native activity boundary.
        if (Device.IsInitialized()) vkDeviceWaitIdle(Device.GetDevice());
        SceneTarget.reset();
        Pipeline.Reset();
        SceneGpu.ResetPipelines();
        FramePacer.Reset();
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
        FramePacer.Initialize(Swapchain.GetSwapchain(),Window);
#if HELENGINE_ANDROID_GAME
        SceneGpu.Initialize(Assets, Swapchain);
        if (SceneRenderScale<1.0) SceneTarget=CreateSceneRenderTarget(SceneRenderScale);
#else
        Pipeline.Initialize(Assets, Swapchain);
#endif
        WindowReady = true;
    }

    void AndroidVulkanRenderer::DetachWindow() {
        WindowReady = false;
        Device.WaitIdle();
        SceneTarget.reset();
        Pipeline.Reset();
        SceneGpu.ResetPipelines();
        FramePacer.Reset();
        Swapchain.Reset();
        if (Surface != VK_NULL_HANDLE) vkDestroySurfaceKHR(Device.GetInstance(), Surface, nullptr);
        Surface = VK_NULL_HANDLE;
        Window = nullptr;
        __android_log_print(ANDROID_LOG_INFO, "HelengineAndroid", "Vulkan surface released.");
    }

    void AndroidVulkanRenderer::RecreateSwapchain() {
        const AndroidTraceSection trace("HelEngine.Vulkan.RecreateSwapchain");
        WindowReady = false;
        Device.WaitIdle();
        SceneTarget.reset();
        Pipeline.Reset();
        SceneGpu.ResetPipelines();
        FramePacer.Reset();
        Swapchain.Reset();
        Swapchain.Initialize(Surface, Window, ContentExtent);
        FramePacer.Initialize(Swapchain.GetSwapchain(),Window);
#if HELENGINE_ANDROID_GAME
        SceneGpu.Initialize(Assets, Swapchain);
        if (SceneRenderScale<1.0) SceneTarget=CreateSceneRenderTarget(SceneRenderScale);
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

    std::unique_ptr<AndroidSceneRenderTarget> AndroidVulkanRenderer::CreateSceneRenderTarget(double scale) {
        auto candidate=std::make_unique<AndroidSceneRenderTarget>(Device);
        candidate->Initialize(Assets,Swapchain,scale);
        return candidate;
    }

    void AndroidVulkanRenderer::SetSceneRenderScale(double scale) {
        ValidateSceneRenderScale(scale);
        if (scale==SceneRenderScale) return;
        std::unique_ptr<AndroidSceneRenderTarget> candidate;
        if (WindowReady) {
            Device.WaitIdle();
            if (scale<1.0) candidate=CreateSceneRenderTarget(scale);
        }
        SceneTarget=std::move(candidate);
        SceneRenderScale=scale;
        const auto physical=Swapchain.GetExtent();
        const auto internal=SceneTarget?SceneTarget->GetExtent():physical;
        __android_log_print(ANDROID_LOG_INFO,"HelengineAndroid","Android 3D render scale applied: scale=%.3f scene=%ux%u output=%ux%u.",
            scale,internal.width,internal.height,physical.width,physical.height);
    }

    void AndroidVulkanRenderer::ValidateSceneRenderScale(double scale) {
        SceneResolutionMath::ValidateScale(scale);
        if (WindowReady && scale<1.0) AndroidSceneRenderTarget::ValidateSupport(Device,Swapchain,scale);
    }

    void AndroidVulkanRenderer::BeginPresentation(VkCommandBuffer command,std::uint32_t image,bool resume) {
        VkClearValue clears[2]{};
        clears[0].color=ClearColor; clears[1].depthStencil={1,0};
        VkRenderPassBeginInfo begin{};
        begin.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        begin.renderPass=resume?Swapchain.GetLoadRenderPass():Swapchain.GetRenderPass();
        begin.framebuffer=Swapchain.GetFramebuffer(image); begin.renderArea.extent=Swapchain.GetExtent();
        begin.clearValueCount=2; begin.pClearValues=clears;
        vkCmdBeginRenderPass(command,&begin,VK_SUBPASS_CONTENTS_INLINE);
    }

    void AndroidVulkanRenderer::RecordSceneFrame(VkCommandBuffer command,std::uint32_t image) {
        if (!SceneTarget) {
            BeginPresentation(command,image,false);
            SceneGpu.Record(command,SceneBatches);
            vkCmdEndRenderPass(command);
            return;
        }
        SceneFramePlanner::Build(SceneBatches,FrameCommands);
        bool clearFrameDepth=true;
        for (const auto& step:FrameCommands) {
            switch (step.Operation) {
                case SceneFrameOperation::BeginPresentation: BeginPresentation(command,image,false); break;
                case SceneFrameOperation::ClearCameraColor:
                    SceneGpu.RecordBatch(command,SceneBatches,step.BatchIndex,Swapchain.GetExtent(),true,false); break;
                case SceneFrameOperation::EndPresentation: vkCmdEndRenderPass(command); break;
                case SceneFrameOperation::BeginWorld:
                    SceneTarget->BeginWorld(command,clearFrameDepth); clearFrameDepth=false; break;
                case SceneFrameOperation::DrawWorld:
                    SceneGpu.RecordBatch(command,SceneBatches,step.BatchIndex,SceneTarget->GetExtent(),false,true); break;
                case SceneFrameOperation::EndWorld: vkCmdEndRenderPass(command); break;
                case SceneFrameOperation::ResumePresentation: BeginPresentation(command,image,true); break;
                case SceneFrameOperation::CompositeWorld: SceneTarget->Composite(command,SceneBatches.at(step.BatchIndex).Viewport); break;
                case SceneFrameOperation::DrawOverlay:
                    SceneGpu.RecordBatch(command,SceneBatches,step.BatchIndex,Swapchain.GetExtent(),true,true); break;
            }
        }
    }

    AndroidMemoryTrimResult AndroidVulkanRenderer::TrimUnusedResources() {
        const AndroidTraceSection trace("HelEngine.Memory.TrimRenderer");
        std::vector<SceneBatch>().swap(SceneBatches);
        std::vector<SceneFrameCommand>().swap(FrameCommands);
        return SceneGpu.TrimUnusedResources();
    }
    void AndroidVulkanRenderer::DrawFrame() {
        const AndroidTraceSection trace("HelEngine.Vulkan.Frame");
        if (!WindowReady) throw std::logic_error("Cannot draw Vulkan before all native-window resources are initialized.");
        const VkDevice device = Device.GetDevice();
        const VkFence fence = Device.GetFrameFence();
        const auto timeout = std::numeric_limits<std::uint64_t>::max();
        {
            const AndroidTraceSection phase("HelEngine.Vulkan.WaitFence");
            check_vulkan(vkWaitForFences(device, 1, &fence, VK_TRUE, timeout), "vkWaitForFences");
        }
#if HELENGINE_ANDROID_GAME
        { const AndroidTraceSection phase("HelEngine.Vulkan.PrepareResources"); SceneGpu.Prepare(SceneBatches); }
#endif
        std::uint32_t image = 0;
        VkResult acquisition;
        {
            const AndroidTraceSection phase("HelEngine.Vulkan.AcquireImage");
            acquisition = vkAcquireNextImageKHR(device, Swapchain.GetSwapchain(), timeout, Device.GetImageAvailable(), VK_NULL_HANDLE, &image);
        }
        if (acquisition == VK_ERROR_OUT_OF_DATE_KHR) {
            RecreateSwapchain();
            const AndroidTraceSection phase("HelEngine.Vulkan.AcquireImage");
            acquisition = vkAcquireNextImageKHR(device, Swapchain.GetSwapchain(), timeout, Device.GetImageAvailable(), VK_NULL_HANDLE, &image);
        }
        if (acquisition != VK_SUBOPTIMAL_KHR) check_vulkan(acquisition, "vkAcquireNextImageKHR");
        const VkCommandBuffer command = Device.GetCommandBuffer();
        {
            const AndroidTraceSection phase("HelEngine.Vulkan.RecordCommands");
            check_vulkan(vkResetCommandBuffer(command, 0), "vkResetCommandBuffer");
            VkCommandBufferBeginInfo begin{};
            begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            check_vulkan(vkBeginCommandBuffer(command, &begin), "vkBeginCommandBuffer");
#if HELENGINE_ANDROID_GAME
            SceneGpu.RecordShadows(command,SceneBatches);
            RecordSceneFrame(command,image);
#else
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
            render.pClearValues = &clear;
            vkCmdBeginRenderPass(command, &render, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline.GetPipeline());
            vkCmdDraw(command, 3, 1, 0, 0);
            vkCmdEndRenderPass(command);
#endif
            check_vulkan(vkEndCommandBuffer(command), "vkEndCommandBuffer");
        }
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
        {
            const AndroidTraceSection phase("HelEngine.Vulkan.QueueSubmit");
            check_vulkan(vkResetFences(device, 1, &fence), "vkResetFences");
            check_vulkan(vkQueueSubmit(Device.GetQueue(), 1, &submit, fence), "vkQueueSubmit");
        }
        const VkSwapchainKHR swapchain = Swapchain.GetSwapchain();
        VkPresentInfoKHR present{};
        present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores = &finished;
        present.swapchainCount = 1;
        present.pSwapchains = &swapchain;
        present.pImageIndices = &image;
        VkResult result;
        { const AndroidTraceSection phase("HelEngine.Vulkan.Present"); result = FramePacer.Present(present); }
        if (result != VK_ERROR_OUT_OF_DATE_KHR && result != VK_SUBOPTIMAL_KHR) check_vulkan(result, "SwappyVk_queuePresent");
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
            __android_log_print(ANDROID_LOG_INFO, "HelengineAndroid", "Vulkan triangle frame presented: %ux%u, frame=%llu.",
                Swapchain.GetExtent().width, Swapchain.GetExtent().height,static_cast<unsigned long long>(++PresentedFrames));
#endif
        }
    }
}
