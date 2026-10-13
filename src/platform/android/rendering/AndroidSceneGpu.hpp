#pragma once
#include "AndroidVulkanDevice.hpp"
#include "AndroidVulkanSwapchain.hpp"
#include <android/asset_manager.h>
#include <memory>
#include <vector>
#include <unordered_map>

namespace helengine::android {
    /// CPU-side clip-space vertex submitted by the generated engine's render visitors.
    struct SceneVertex {
        float Position[4];
        float Color[4];
        float TexCoord[2];
    };
    /// RGBA8 image retained by a runtime texture, with a revision for dynamic font-atlas updates.
    struct SceneTexture {
        std::uint32_t Width;
        std::uint32_t Height;
        std::uint64_t Revision = 1;
        std::vector<std::uint8_t> Pixels;
    };
    /// Ordered triangle batch carrying its viewport, texture, and 3D depth policy.
    struct SceneBatch {
        std::vector<SceneVertex> Vertices;
        std::shared_ptr<SceneTexture> Texture;
        VkRect2D Viewport;
        bool DepthTest;
    };
    /// Uploads authored geometry and font atlases and records Vulkan draws in engine queue order.
    class AndroidSceneGpu {
    public:
        /// Binds GPU resources to the renderer's persistent device.
        explicit AndroidSceneGpu(AndroidVulkanDevice& device);
        /// Releases all device resources after pending work completes.
        ~AndroidSceneGpu();
        /// Creates pipeline and descriptor resources compatible with the current swapchain.
        void Initialize(AAssetManager* assets, const AndroidVulkanSwapchain& swapchain);
        /// Releases only pipelines tied to the swapchain, preserving resident textures and vertex storage.
        void ResetPipelines();
        /// Uploads the next frame's data after its preceding fence has completed.
        void Prepare(const std::vector<SceneBatch>& batches);
        /// Records triangle batches into the active render pass.
        void Record(VkCommandBuffer command, const std::vector<SceneBatch>& batches);
    private:
        /// Device-local sampled image and descriptor whose source lifetime is tracked without retaining scene assets.
        struct TextureResource {
            std::weak_ptr<SceneTexture> Source;
            std::uint64_t Revision = 0;
            VkImage Image = VK_NULL_HANDLE;
            VkDeviceMemory Memory = VK_NULL_HANDLE;
            VkImageView View = VK_NULL_HANDLE;
            VkDescriptorSet Descriptor = VK_NULL_HANDLE;
        };
        /// Allocates a coherent buffer and exposes its writable mapping.
        void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer& buffer, VkDeviceMemory& memory, void*& mapped);
        /// Uploads a complete RGBA8 image and transitions it for fragment sampling.
        void UploadTexture(const std::shared_ptr<SceneTexture>& source, TextureResource& texture);
        /// Frees a sampled image and returns its descriptor to the pool.
        void DestroyTexture(TextureResource& texture);
        /// Loads and validates a packaged SPIR-V shader.
        VkShaderModule LoadShader(AAssetManager* assets, const char* path);
        /// Persistent logical device owned by the parent renderer.
        AndroidVulkanDevice& Device;
        /// Frame vertex buffer reused only after the submission fence signals.
        VkBuffer VertexBuffer = VK_NULL_HANDLE;
        /// Host-coherent storage backing the vertex buffer.
        VkDeviceMemory VertexMemory = VK_NULL_HANDLE;
        /// Persistent CPU mapping into frame vertex storage.
        void* VertexMapping = nullptr;
        /// Allocated byte capacity of frame vertex storage.
        VkDeviceSize VertexCapacity = 0;
        /// Layout exposing one combined sampled image to the fragment shader.
        VkDescriptorSetLayout DescriptorLayout = VK_NULL_HANDLE;
        /// Pool supporting individually freed texture descriptors.
        VkDescriptorPool DescriptorPool = VK_NULL_HANDLE;
        /// Shared linear clamp sampler for font atlases and the white solid-color image.
        VkSampler Sampler = VK_NULL_HANDLE;
        /// Layout used by both depth-enabled meshes and overlay triangles.
        VkPipelineLayout Layout = VK_NULL_HANDLE;
        /// Depth-tested opaque mesh pipeline.
        VkPipeline MeshPipeline = VK_NULL_HANDLE;
        /// Alpha-blended overlay pipeline with depth testing disabled.
        VkPipeline OverlayPipeline = VK_NULL_HANDLE;
        /// Command pool for synchronous load-time image uploads.
        VkCommandPool UploadPool = VK_NULL_HANDLE;
        /// Resident image resources indexed by stable source addresses.
        std::unordered_map<const SceneTexture*, TextureResource> Textures;
        /// Single white image sampled by untextured batches.
        std::shared_ptr<SceneTexture> White;
    };
}
