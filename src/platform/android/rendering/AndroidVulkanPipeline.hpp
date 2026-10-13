#pragma once

#include "AndroidVulkanDevice.hpp"
#include "AndroidVulkanSwapchain.hpp"
#include <android/asset_manager.h>

namespace helengine::android {
    /// Owns the graphics pipeline for the native Vulkan triangle probe and its packaged SPIR-V modules.
    class AndroidVulkanPipeline {
    public:
        /// Associates pipeline resources with the renderer's longer-lived Vulkan device.
        explicit AndroidVulkanPipeline(AndroidVulkanDevice& device);
        /// Releases the pipeline, layout, and shader modules before device destruction.
        ~AndroidVulkanPipeline();
        /// Prevents copying pipeline resource ownership.
        AndroidVulkanPipeline(const AndroidVulkanPipeline&) = delete;
        /// Prevents assigning pipeline resource ownership.
        AndroidVulkanPipeline& operator=(const AndroidVulkanPipeline&) = delete;
        /// Loads the packaged shaders and creates a pipeline compatible with the swapchain render pass.
        void Initialize(AAssetManager* assets, const AndroidVulkanSwapchain& swapchain);
        /// Releases resources after the renderer has waited for any referencing commands.
        void Reset();
        /// Gets the graphics pipeline bound when drawing the native probe triangle.
        VkPipeline GetPipeline() const { return Pipeline; }

    private:
        /// Creates an aligned Vulkan shader module from a complete packaged SPIR-V asset.
        VkShaderModule LoadShader(AAssetManager* assets, const char* path);
        /// Device that outlives the pipeline and shader resources.
        AndroidVulkanDevice& Device;
        /// Pipeline layout without descriptors for the vertex-index-based probe.
        VkPipelineLayout Layout = VK_NULL_HANDLE;
        /// Graphics pipeline compiled for the current render pass and extent.
        VkPipeline Pipeline = VK_NULL_HANDLE;
        /// Vertex shader generating the diagnostic triangle from vertex indices.
        VkShaderModule VertexShader = VK_NULL_HANDLE;
        /// Fragment shader interpolating the diagnostic vertex colors.
        VkShaderModule FragmentShader = VK_NULL_HANDLE;
    };
}
