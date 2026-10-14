#pragma once
#include "AndroidVulkanDevice.hpp"
#include "AndroidVulkanSwapchain.hpp"
#include <android/asset_manager.h>

namespace helengine::android {
    /// Owns scaled world color/depth and the premultiplied composite pipeline, independently of native-resolution UI.
    class AndroidSceneRenderTarget {
    public:
        /// Binds target resource lifetime to the persistent renderer device.
        explicit AndroidSceneRenderTarget(AndroidVulkanDevice& device);
        /// Releases completed target work; the renderer waits for the GPU before replacing or destroying this owner.
        ~AndroidSceneRenderTarget();
        /// Prevents copying GPU handle ownership.
        AndroidSceneRenderTarget(const AndroidSceneRenderTarget&)=delete;
        /// Prevents assigning GPU handle ownership.
        AndroidSceneRenderTarget& operator=(const AndroidSceneRenderTarget&)=delete;
        /// Checks scaling and sampled-attachment support without allocating or mutating resources.
        static void ValidateSupport(AndroidVulkanDevice& device,const AndroidVulkanSwapchain& swapchain,double scale);
        /// Builds a complete candidate target; failure leaves the renderer's previous target untouched.
        void Initialize(AAssetManager* assets,const AndroidVulkanSwapchain& swapchain,double scale);
        /// Begins transparent world color while preserving depth between this frame's cameras.
        void BeginWorld(VkCommandBuffer command,bool clearFrameDepth);
        /// Composites only the camera region onto the preserved native-resolution presentation image.
        void Composite(VkCommandBuffer command,VkRect2D cameraViewport);
        /// Gets the internal pixel dimensions without changing the engine's physical window size.
        VkExtent2D GetExtent() const { return Extent; }
    private:
        /// Allocates an attachment and its view, leaving partial handles owned for exception cleanup.
        void CreateAttachment(VkFormat format,VkImageUsageFlags usage,VkImageAspectFlags aspect,
            VkImage& image,VkDeviceMemory& memory,VkImageView& view);
        /// Creates a world render pass compatible with the scene mesh pipelines.
        void CreateWorldPass(VkFormat colorFormat);
        /// Builds the native-resolution fullscreen composite pipeline and sampled-color descriptor.
        void CreateComposite(AAssetManager* assets,VkRenderPass presentationPass);
        /// Reads a required compiled shader from the APK.
        VkShaderModule LoadShader(AAssetManager* assets,const char* path);
        /// Releases every partially or fully initialized resource without throwing.
        void Reset();
        /// Device outlives this target and all of its resources.
        AndroidVulkanDevice& Device;
        /// Rounded internal color and depth dimensions.
        VkExtent2D Extent{};
        /// Unscaled output dimensions used by the fullscreen composite viewport.
        VkExtent2D OutputExtent{};
        /// Sampled world color attachment.
        VkImage Color=VK_NULL_HANDLE;
        /// Device-local world color storage.
        VkDeviceMemory ColorMemory=VK_NULL_HANDLE;
        /// Color attachment and sampler view.
        VkImageView ColorView=VK_NULL_HANDLE;
        /// World depth reused between cameras until the next frame's initial clear.
        VkImage Depth=VK_NULL_HANDLE;
        /// Device-local world depth storage.
        VkDeviceMemory DepthMemory=VK_NULL_HANDLE;
        /// World depth attachment view.
        VkImageView DepthView=VK_NULL_HANDLE;
        /// World pass transitions color to shader-readable layout after each camera.
        VkRenderPass WorldPass=VK_NULL_HANDLE;
        /// Pairs the scaled color and depth attachments.
        VkFramebuffer Framebuffer=VK_NULL_HANDLE;
        /// Layout declaring the world color sampler.
        VkDescriptorSetLayout DescriptorLayout=VK_NULL_HANDLE;
        /// Owns the sampled-color descriptor allocation.
        VkDescriptorPool DescriptorPool=VK_NULL_HANDLE;
        /// Selects the completed world color for compositing.
        VkDescriptorSet Descriptor=VK_NULL_HANDLE;
        /// Linear clamp sampler used for the resolution upscale.
        VkSampler Sampler=VK_NULL_HANDLE;
        /// Fullscreen composite shader interface.
        VkPipelineLayout Layout=VK_NULL_HANDLE;
        /// Premultiplied world coverage blended over native-resolution content.
        VkPipeline Pipeline=VK_NULL_HANDLE;
        /// Tracks the first depth transition of this resource lifetime.
        bool DepthInitialized=false;
    };
}
