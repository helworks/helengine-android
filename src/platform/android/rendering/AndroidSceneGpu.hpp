#pragma once
#include "AndroidVulkanDevice.hpp"
#include "AndroidVulkanSwapchain.hpp"
#include "AndroidSceneLighting.hpp"
#include "AndroidShadowMap.hpp"
#include "AndroidMemoryTrimResult.hpp"
#include <android/asset_manager.h>
#include <memory>
#include <vector>
#include <unordered_map>
#include <array>

namespace helengine::android {
    /// CPU-side clip-space vertex submitted by the generated engine's render visitors.
    struct SceneVertex {
        float Position[4];
        float Color[4];
        float TexCoord[2];
        /// World-space attributes interpolated for per-pixel mesh lighting; overlay vertices leave them zero.
        float WorldPosition[3]{};
        float Normal[3]{};
    };
    /// RGBA8 image retained by a runtime texture, with a revision for dynamic font-atlas updates.
    struct SceneTexture {
        std::uint32_t Width;
        std::uint32_t Height;
        std::uint64_t Revision = 1;
        std::vector<std::uint8_t> Pixels;
        /// Diffuse material textures decode sRGB at sampling; atlases and scalar maps retain linear bytes.
        bool Srgb=false;
        /// Material UVs repeat while sprite source rectangles and font atlases clamp to their bounds.
        bool Repeat=false;
    };
    /// Ordered triangle batch carrying its viewport, texture, and 3D depth policy.
    struct SceneBatch {
        /// Distinguishes world geometry and depth clears from native-resolution overlays.
        bool WorldSpace=false;
        /// Delimits camera order even when its clear policies are disabled or it contains no meshes.
        bool BeginCamera=false;
        std::vector<SceneVertex> Vertices;
        std::shared_ptr<SceneTexture> Texture;
        VkRect2D Viewport;
        bool DepthTest;
        /// Optional intersection of authored UI clip regions in physical viewport pixels.
        VkRect2D Scissor{};
        /// Distinguishes an authored empty clip from a batch using the full camera viewport.
        bool HasScissor=false;
        /// Camera clear commands retain their position in the ordered frame even without geometry.
        bool ClearColor=false;
        /// Clears camera depth independently of earlier cameras and overlays.
        bool ClearDepth=false;
        /// Authored RGBA camera clear color.
        float ClearValue[4]{};
        /// Authored depth clear, normally one for a forward depth buffer.
        float ClearDepthValue=1;
        /// Optional roughness map bound independently of the diffuse image.
        std::shared_ptr<SceneTexture> RoughnessTexture;
        /// Camera, lights and surface parameters retained in the same order as this geometry.
        SceneLighting Lighting{};
        /// Blended meshes test existing depth without writing transparent coverage into it.
        bool Transparent=false;
        /// Whether this geometry contributes to the directional depth map.
        bool CastShadow=false;
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
        /// Waits for device work, releases rebuildable frame arenas and retires textures without a live source owner.
        AndroidMemoryTrimResult TrimUnusedResources();
        /// Uploads the next frame's data after its preceding fence has completed.
        void Prepare(const std::vector<SceneBatch>& batches);
        /// Records triangle batches into the active render pass.
        void Record(VkCommandBuffer command, const std::vector<SceneBatch>& batches);
        /// Records one original batch into a physical or scaled target without changing its geometry or uniform offsets.
        void RecordBatch(VkCommandBuffer command,const std::vector<SceneBatch>& batches,std::size_t index,
            VkExtent2D target,bool allowColorClear,bool allowDepthClear);
        /// Renders directional caster geometry before the presentation render pass begins.
        void RecordShadows(VkCommandBuffer command,const std::vector<SceneBatch>& batches);
    private:
        /// Lets isolated native resource tests seed driver handles without adding a public runtime mutation API.
        friend class AndroidSceneGpuMemoryTests;
        /// Releases and clears both coherent frame arenas so the next Prepare recreates correctly sized buffers.
        void ReleaseFrameBuffers();
        /// Retires expired texture cache entries only after callers have made GPU resource destruction safe.
        uint64_t PruneExpiredTextures();
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
        /// Maps camera or clip edges through rounded target dimensions, retaining explicit empty clips.
        static VkRect2D MapRectangle(VkRect2D rectangle,VkExtent2D source,VkExtent2D target);
        /// Persistent logical device owned by the parent renderer.
        AndroidVulkanDevice& Device;
        /// Fixed-resolution directional depth resources retained across surface replacement.
        AndroidShadowMap Shadows;
        /// Sampled-depth descriptor bound by the scene fragment shader.
        VkDescriptorSet ShadowDescriptor=VK_NULL_HANDLE;
        /// Projection written into each lit batch's uniform block during preparation.
        std::array<float,16> ShadowMatrix{};
        /// Whether the current frame contains an enabled directional shadow source and casters.
        bool HasShadows=false;
        /// Records the initial clear and read-only transition required by the bound shadow descriptor, even when shadow drawing is disabled.
        bool ShadowMapReady=false;
        /// Fits one orthographic light projection to the current eligible world-space vertices.
        void FitShadowProjection(const std::vector<SceneBatch>& batches);
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
        /// Linear repeat sampler for authored material tiling.
        VkSampler RepeatSampler = VK_NULL_HANDLE;
        /// Layout used by both depth-enabled meshes and overlay triangles.
        VkPipelineLayout Layout = VK_NULL_HANDLE;
        /// Depth-tested opaque mesh pipeline.
        VkPipeline MeshPipeline = VK_NULL_HANDLE;
        /// Alpha-blended overlay pipeline with depth testing disabled.
        VkPipeline OverlayPipeline = VK_NULL_HANDLE;
        /// Depth-tested, alpha-blended mesh pipeline with depth writes disabled.
        VkPipeline TransparentPipeline = VK_NULL_HANDLE;
        /// Dynamic uniform layout shared by per-draw camera, light and surface data.
        VkDescriptorSetLayout LightingLayout = VK_NULL_HANDLE;
        /// Descriptor selecting the current frame's reusable lighting buffer.
        VkDescriptorSet LightingDescriptor = VK_NULL_HANDLE;
        /// Host-coherent aligned per-batch std140 lighting storage.
        VkBuffer LightingBuffer = VK_NULL_HANDLE;
        VkDeviceMemory LightingMemory = VK_NULL_HANDLE;
        void* LightingMapping = nullptr;
        VkDeviceSize LightingCapacity=0;
        VkDeviceSize LightingStride=0;
        /// Attachment encoding copied from each replacement swapchain.
        bool SurfaceSrgb=false;
        /// Original display size retained for full-resolution layout, projection and overlay coordinates.
        VkExtent2D OutputExtent{};
        /// Original vertex-buffer offsets indexed identically to each prepared batch's uniform data.
        std::vector<std::uint32_t> FirstVertices;
        /// Command pool for synchronous load-time image uploads.
        VkCommandPool UploadPool = VK_NULL_HANDLE;
        /// Resident image resources indexed by stable source addresses.
        std::unordered_map<const SceneTexture*, TextureResource> Textures;
        /// Single white image sampled by untextured batches.
        std::shared_ptr<SceneTexture> White;
    };
}
