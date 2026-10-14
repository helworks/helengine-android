#pragma once

#include "AndroidVulkanDevice.hpp"
#include "AndroidVulkanSwapchain.hpp"
#include "AndroidVulkanPipeline.hpp"
#include "AndroidSceneGpu.hpp"
#include "AndroidSceneRenderTarget.hpp"
#include "SceneFrameCommand.hpp"
#include "AndroidFramePacer.hpp"

namespace helengine::android {
    /// Presents the native Vulkan probe while keeping device and surface ownership separate across Android lifecycle events.
    class AndroidVulkanRenderer {
    public:
        /// Borrows the live native activity for packaged assets and frame-pacing JNI access.
        explicit AndroidVulkanRenderer(ANativeActivity* activity);
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
        /// Clears retained frame geometry and releases rebuildable GPU arenas on the renderer's owning thread.
        AndroidMemoryTrimResult TrimUnusedResources();
        /// Returns current pixel dimensions for engine window and camera layout.
        VkExtent2D GetExtent() const { return Swapchain.GetExtent(); }
        /// Removes the preceding engine frame's queued geometry before collecting the next draw.
        void BeginSceneFrame() { SceneBatches.clear(); }
        /// Appends one authored drawable's triangles in render-queue order.
        void Submit(SceneBatch batch) { SceneBatches.push_back(std::move(batch)); }
        /// Uses the active scene camera's clear color for game frames.
        void SetClearColor(float r, float g, float b, float a) { ClearColor = {{r,g,b,a}}; }
        /// Applies a validated quality value synchronously, publishing it only after replacement resources are ready.
        void SetSceneRenderScale(double scale);
        /// Preflights quality and attachment support without changing the active scale or allocating a target.
        void ValidateSceneRenderScale(double scale);
        /// Gets the active 3D quality scale; physical window dimensions and overlays retain their own native resolution.
        double GetSceneRenderScale() const { return SceneRenderScale; }
        /// Applies the engine user's FPS request, retaining it across native surface replacement.
        void SetTargetFrameRate(double framesPerSecond) { FramePacer.SetTargetFrameRate(framesPerSecond); }
        /// Discards stale presentation history after Android resumes the suspended game.
        void ResetFrameTiming() { FramePacer.ResetTiming(); }

    private:
        /// Recreates the swapchain and its graphics pipeline while preserving the device and surface.
        void RecreateSwapchain();
        /// Creates an independently owned target candidate without mutating the renderer's active configuration.
        std::unique_ptr<AndroidSceneRenderTarget> CreateSceneRenderTarget(double scale);
        /// Opens either the initial clear pass or a compatible continuation preserving native-resolution content.
        void BeginPresentation(VkCommandBuffer command,std::uint32_t image,bool resume);
        /// Records world and overlay passes in the engine's original camera order.
        void RecordSceneFrame(VkCommandBuffer command,std::uint32_t image);
        /// Converts a specified sRGB byte channel to the linear value required by an sRGB attachment.
        static float ToLinearColor(std::uint8_t channel);
        /// Activity-owned asset manager used only while the renderer's activity remains alive.
        AAssetManager* Assets;
        /// Device and frame resources that outlive the surface-sized resources.
        AndroidVulkanDevice Device;
        /// Swappy and its JNI attachment outlive the swapchain but are destroyed before the Vulkan device.
        AndroidFramePacer FramePacer;
        /// Surface-sized images, render targets, and presentation semaphores.
        AndroidVulkanSwapchain Swapchain;
        /// Probe graphics pipeline compatible with the current swapchain.
        AndroidVulkanPipeline Pipeline;
        /// Persistent geometry and image resources for authored scenes.
        AndroidSceneGpu SceneGpu;
        /// Optional reduced-resolution world target; absent at full quality to keep the direct render path.
        std::unique_ptr<AndroidSceneRenderTarget> SceneTarget;
        /// Quality chosen by the engine user or gameplay settings API, retained across native surface replacement.
        double SceneRenderScale=1.0;
        /// Reusable CPU plan preserves per-camera clears, meshes and native overlays.
        std::vector<SceneFrameCommand> FrameCommands;
        /// Ordered geometry gathered by the engine's 3D and 2D render visitors.
        std::vector<SceneBatch> SceneBatches;
        /// Active camera clear color in the engine's color convention.
        VkClearColorValue ClearColor{{100.0f/255,149.0f/255,237.0f/255,1}};
        /// Number of successfully presented frames, used by game and standalone probe diagnostics.
        std::uint64_t PresentedFrames = 0;
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
