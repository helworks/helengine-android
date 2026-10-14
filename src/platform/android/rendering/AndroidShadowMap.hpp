#pragma once
#include "AndroidVulkanDevice.hpp"

namespace helengine::android {
    /// Owns the sampled directional depth target independently of Android presentation surfaces.
    class AndroidShadowMap {
    public:
        /// Binds depth resources to the renderer's persistent Vulkan device.
        explicit AndroidShadowMap(AndroidVulkanDevice& device) : Device(device) {}
        /// Releases the pipeline, render target and sampler after device work has completed.
        ~AndroidShadowMap();
        /// Creates the fixed-resolution depth target and a pipeline matching the shared scene descriptor layout.
        void Initialize(VkPipelineLayout layout,VkShaderModule vertex,VkShaderModule fragment);
        /// Releases only the pipeline before the shared pipeline layout is replaced.
        void ResetPipeline();
        /// Starts a depth-only pass and binds its full-map viewport and pipeline.
        void Begin(VkCommandBuffer command);
        /// Completes the depth pass and makes its image available to fragment sampling.
        void End(VkCommandBuffer command);
        /// Gets the descriptor image/sampler in the layout published by the shadow pass.
        VkDescriptorImageInfo GetDescriptor() const { return {Sampler,View,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL}; }
        /// Fixed map resolution used by the shader's nine-tap PCF kernel.
        static constexpr std::uint32_t Size=1024;
    private:
        /// Device outlives every owned shadow resource.
        AndroidVulkanDevice& Device;
        /// Depth-only attachment also sampled by scene fragments.
        VkImage Image=VK_NULL_HANDLE;
        /// Device-local backing storage for the depth attachment.
        VkDeviceMemory Memory=VK_NULL_HANDLE;
        /// Depth aspect view shared by the framebuffer and comparison sampler.
        VkImageView View=VK_NULL_HANDLE;
        /// Nearest comparison sampler; filtering is implemented by the explicit PCF kernel.
        VkSampler Sampler=VK_NULL_HANDLE;
        /// Render pass with dependencies ordering depth writes and subsequent fragment reads.
        VkRenderPass Pass=VK_NULL_HANDLE;
        /// Fixed-resolution framebuffer independent of the display extent.
        VkFramebuffer Framebuffer=VK_NULL_HANDLE;
        /// Alpha-tested depth pipeline using the scene geometry and material descriptors.
        VkPipeline Pipeline=VK_NULL_HANDLE;
    };
}
