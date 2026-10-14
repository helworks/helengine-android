#include "AndroidSceneRenderTarget.hpp"
#include "AndroidRenderPassDependencies.hpp"
#include "SceneResolutionMath.hpp"
#include "vulkan_memory.hpp"
#include <memory>
#include <vector>

namespace helengine::android {
    AndroidSceneRenderTarget::AndroidSceneRenderTarget(AndroidVulkanDevice& device) : Device(device) {}
    AndroidSceneRenderTarget::~AndroidSceneRenderTarget() { Reset(); }

    void AndroidSceneRenderTarget::Initialize(AAssetManager* assets,const AndroidVulkanSwapchain& swapchain,double scale) {
        ValidateSupport(Device,swapchain,scale);
        OutputExtent=swapchain.GetExtent();
        Extent={SceneResolutionMath::ScaleDimension(OutputExtent.width,scale),SceneResolutionMath::ScaleDimension(OutputExtent.height,scale)};
        CreateAttachment(swapchain.GetFormat(),VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_IMAGE_ASPECT_COLOR_BIT,
            Color,ColorMemory,ColorView);
        CreateAttachment(VK_FORMAT_D32_SFLOAT,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,VK_IMAGE_ASPECT_DEPTH_BIT,
            Depth,DepthMemory,DepthView);
        CreateWorldPass(swapchain.GetFormat());
        VkImageView views[]{ColorView,DepthView};
        VkFramebufferCreateInfo framebuffer{};
        framebuffer.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer.renderPass=WorldPass; framebuffer.attachmentCount=2; framebuffer.pAttachments=views;
        framebuffer.width=Extent.width; framebuffer.height=Extent.height; framebuffer.layers=1;
        check_vulkan(vkCreateFramebuffer(Device.GetDevice(),&framebuffer,nullptr,&Framebuffer),"vkCreateFramebuffer(scene resolution)");
        CreateComposite(assets,swapchain.GetRenderPass());
    }

    void AndroidSceneRenderTarget::ValidateSupport(AndroidVulkanDevice& device,const AndroidVulkanSwapchain& swapchain,double scale) {
        SceneResolutionMath::ValidateScale(scale);
        VkFormatProperties properties{};
        vkGetPhysicalDeviceFormatProperties(device.GetPhysicalDevice(),swapchain.GetFormat(),&properties);
        const VkFormatFeatureFlags required=VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT|
            VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
        if ((properties.optimalTilingFeatures&required)!=required) throw std::runtime_error("Android surface format cannot support a linearly sampled scene target.");
        VkImageFormatProperties image{};
        check_vulkan(vkGetPhysicalDeviceImageFormatProperties(device.GetPhysicalDevice(),swapchain.GetFormat(),VK_IMAGE_TYPE_2D,
            VK_IMAGE_TILING_OPTIMAL,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,0,&image),"vkGetPhysicalDeviceImageFormatProperties(scene resolution)");
        const auto physical=swapchain.GetExtent();
        if (SceneResolutionMath::ScaleDimension(physical.width,scale)>image.maxExtent.width ||
            SceneResolutionMath::ScaleDimension(physical.height,scale)>image.maxExtent.height) {
            throw std::runtime_error("Android scene resolution exceeds sampled attachment limits.");
        }
    }

    void AndroidSceneRenderTarget::CreateAttachment(VkFormat format,VkImageUsageFlags usage,VkImageAspectFlags aspect,
        VkImage& image,VkDeviceMemory& memory,VkImageView& view) {
        VkImageCreateInfo create{};
        create.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO; create.imageType=VK_IMAGE_TYPE_2D;
        create.format=format; create.extent={Extent.width,Extent.height,1}; create.mipLevels=1; create.arrayLayers=1;
        create.samples=VK_SAMPLE_COUNT_1_BIT; create.tiling=VK_IMAGE_TILING_OPTIMAL; create.usage=usage;
        check_vulkan(vkCreateImage(Device.GetDevice(),&create,nullptr,&image),"vkCreateImage(scene resolution)");
        memory=AllocateImageMemory(Device,image);
        VkImageViewCreateInfo imageView{};
        imageView.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO; imageView.image=image; imageView.viewType=VK_IMAGE_VIEW_TYPE_2D;
        imageView.format=format; imageView.subresourceRange={aspect,0,1,0,1};
        check_vulkan(vkCreateImageView(Device.GetDevice(),&imageView,nullptr,&view),"vkCreateImageView(scene resolution)");
    }

    void AndroidSceneRenderTarget::CreateWorldPass(VkFormat colorFormat) {
        VkAttachmentDescription attachments[2]{};
        attachments[0].format=colorFormat; attachments[0].samples=VK_SAMPLE_COUNT_1_BIT;
        attachments[0].loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR; attachments[0].storeOp=VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE; attachments[0].stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout=VK_IMAGE_LAYOUT_UNDEFINED; attachments[0].finalLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        attachments[1]=attachments[0]; attachments[1].format=VK_FORMAT_D32_SFLOAT;
        attachments[1].loadOp=VK_ATTACHMENT_LOAD_OP_LOAD;
        attachments[1].initialLayout=attachments[1].finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        VkAttachmentReference color{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference depth{1,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        VkSubpassDescription subpass{}; subpass.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount=1; subpass.pColorAttachments=&color; subpass.pDepthStencilAttachment=&depth;
        const auto dependencies=AndroidRenderPassDependencies::Create();
        VkRenderPassCreateInfo pass{};
        pass.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO; pass.attachmentCount=2; pass.pAttachments=attachments;
        pass.subpassCount=1; pass.pSubpasses=&subpass; pass.dependencyCount=dependencies.size(); pass.pDependencies=dependencies.data();
        check_vulkan(vkCreateRenderPass(Device.GetDevice(),&pass,nullptr,&WorldPass),"vkCreateRenderPass(scene resolution)");
    }

    VkShaderModule AndroidSceneRenderTarget::LoadShader(AAssetManager* assets,const char* path) {
        std::unique_ptr<AAsset,decltype(&AAsset_close)> asset(AAssetManager_open(assets,path,AASSET_MODE_BUFFER),AAsset_close);
        if (!asset) throw std::runtime_error(std::string("Missing composite shader: ")+path);
        const auto size=AAsset_getLength(asset.get());
        if (size<20 || size%4) throw std::runtime_error("Invalid composite SPIR-V size.");
        std::vector<std::uint32_t> words(size/4);
        if (AAsset_read(asset.get(),words.data(),size)!=size || words[0]!=0x07230203) throw std::runtime_error("Invalid composite SPIR-V data.");
        VkShaderModuleCreateInfo shader{};
        shader.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO; shader.codeSize=size; shader.pCode=words.data();
        VkShaderModule result=VK_NULL_HANDLE;
        check_vulkan(vkCreateShaderModule(Device.GetDevice(),&shader,nullptr,&result),"vkCreateShaderModule(composite)");
        return result;
    }

    void AndroidSceneRenderTarget::CreateComposite(AAssetManager* assets,VkRenderPass presentationPass) {
        const auto device=Device.GetDevice();
        VkDescriptorSetLayoutBinding binding{0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr};
        VkDescriptorSetLayoutCreateInfo descriptorLayout{};
        descriptorLayout.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorLayout.bindingCount=1; descriptorLayout.pBindings=&binding;
        check_vulkan(vkCreateDescriptorSetLayout(device,&descriptorLayout,nullptr,&DescriptorLayout),"vkCreateDescriptorSetLayout(composite)");
        VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1};
        VkDescriptorPoolCreateInfo pool{};
        pool.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO; pool.maxSets=1; pool.poolSizeCount=1; pool.pPoolSizes=&size;
        check_vulkan(vkCreateDescriptorPool(device,&pool,nullptr,&DescriptorPool),"vkCreateDescriptorPool(composite)");
        VkDescriptorSetAllocateInfo allocate{};
        allocate.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO; allocate.descriptorPool=DescriptorPool;
        allocate.descriptorSetCount=1; allocate.pSetLayouts=&DescriptorLayout;
        check_vulkan(vkAllocateDescriptorSets(device,&allocate,&Descriptor),"vkAllocateDescriptorSets(composite)");
        VkSamplerCreateInfo sampler{};
        sampler.sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO; sampler.minFilter=sampler.magFilter=VK_FILTER_LINEAR;
        sampler.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler.addressModeU=sampler.addressModeV=sampler.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        check_vulkan(vkCreateSampler(device,&sampler,nullptr,&Sampler),"vkCreateSampler(composite)");
        VkDescriptorImageInfo image{Sampler,ColorView,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkWriteDescriptorSet write{};
        write.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; write.dstSet=Descriptor; write.descriptorCount=1;
        write.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; write.pImageInfo=&image;
        vkUpdateDescriptorSets(device,1,&write,0,nullptr);
        VkPipelineLayoutCreateInfo layout{};
        layout.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO; layout.setLayoutCount=1; layout.pSetLayouts=&DescriptorLayout;
        check_vulkan(vkCreatePipelineLayout(device,&layout,nullptr,&Layout),"vkCreatePipelineLayout(composite)");
        VkShaderModule vertex=LoadShader(assets,"shaders/composite.vert.spv"),fragment=VK_NULL_HANDLE;
        try {
            fragment=LoadShader(assets,"shaders/composite.frag.spv");
            VkPipelineShaderStageCreateInfo stages[2]{};
            stages[0].sType=stages[1].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT; stages[0].module=vertex; stages[0].pName="main";
            stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module=fragment; stages[1].pName="main";
            VkPipelineVertexInputStateCreateInfo vertices{}; vertices.sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            VkPipelineInputAssemblyStateCreateInfo assembly{};
            assembly.sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO; assembly.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            VkPipelineViewportStateCreateInfo viewport{};
            viewport.sType=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO; viewport.viewportCount=viewport.scissorCount=1;
            VkDynamicState states[]{VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};
            VkPipelineDynamicStateCreateInfo dynamic{};
            dynamic.sType=VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO; dynamic.dynamicStateCount=2; dynamic.pDynamicStates=states;
            VkPipelineRasterizationStateCreateInfo raster{};
            raster.sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO; raster.polygonMode=VK_POLYGON_MODE_FILL;
            raster.cullMode=VK_CULL_MODE_NONE; raster.lineWidth=1;
            VkPipelineMultisampleStateCreateInfo samples{};
            samples.sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO; samples.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
            VkPipelineDepthStencilStateCreateInfo depth{}; depth.sType=VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            VkPipelineColorBlendAttachmentState color{};
            color.blendEnable=VK_TRUE; color.colorWriteMask=15;
            color.srcColorBlendFactor=color.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE;
            color.dstColorBlendFactor=color.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            color.colorBlendOp=color.alphaBlendOp=VK_BLEND_OP_ADD;
            VkPipelineColorBlendStateCreateInfo blend{};
            blend.sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO; blend.attachmentCount=1; blend.pAttachments=&color;
            VkGraphicsPipelineCreateInfo pipeline{};
            pipeline.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO; pipeline.stageCount=2; pipeline.pStages=stages;
            pipeline.pVertexInputState=&vertices; pipeline.pInputAssemblyState=&assembly; pipeline.pViewportState=&viewport;
            pipeline.pDynamicState=&dynamic; pipeline.pRasterizationState=&raster; pipeline.pMultisampleState=&samples;
            pipeline.pDepthStencilState=&depth; pipeline.pColorBlendState=&blend; pipeline.layout=Layout; pipeline.renderPass=presentationPass;
            check_vulkan(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&pipeline,nullptr,&Pipeline),"vkCreateGraphicsPipelines(composite)");
        } catch (...) {
            if (fragment) vkDestroyShaderModule(device,fragment,nullptr);
            vkDestroyShaderModule(device,vertex,nullptr); throw;
        }
        vkDestroyShaderModule(device,fragment,nullptr); vkDestroyShaderModule(device,vertex,nullptr);
    }

    void AndroidSceneRenderTarget::BeginWorld(VkCommandBuffer command,bool clearFrameDepth) {
        if (!DepthInitialized) {
            VkImageMemoryBarrier barrier{};
            barrier.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER; barrier.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
            barrier.image=Depth; barrier.subresourceRange={VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,1};
            barrier.dstAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,0,0,nullptr,0,nullptr,1,&barrier);
            DepthInitialized=true;
        }
        VkClearValue transparent{};
        VkRenderPassBeginInfo begin{};
        begin.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO; begin.renderPass=WorldPass; begin.framebuffer=Framebuffer;
        begin.renderArea.extent=Extent; begin.clearValueCount=1; begin.pClearValues=&transparent;
        vkCmdBeginRenderPass(command,&begin,VK_SUBPASS_CONTENTS_INLINE);
        if (clearFrameDepth) {
            VkClearAttachment clear{}; clear.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT; clear.clearValue.depthStencil={1,0};
            VkClearRect rectangle{{{0,0},Extent},0,1};
            vkCmdClearAttachments(command,1,&clear,1,&rectangle);
        }
    }

    void AndroidSceneRenderTarget::Composite(VkCommandBuffer command,VkRect2D cameraViewport) {
        VkViewport viewport{0,0,static_cast<float>(OutputExtent.width),static_cast<float>(OutputExtent.height),0,1};
        vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,Pipeline);
        vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_GRAPHICS,Layout,0,1,&Descriptor,0,nullptr);
        vkCmdSetViewport(command,0,1,&viewport); vkCmdSetScissor(command,0,1,&cameraViewport);
        vkCmdDraw(command,3,1,0,0);
    }

    void AndroidSceneRenderTarget::Reset() {
        const auto device=Device.GetDevice();
        if (Pipeline) vkDestroyPipeline(device,Pipeline,nullptr);
        if (Layout) vkDestroyPipelineLayout(device,Layout,nullptr);
        if (DescriptorPool) vkDestroyDescriptorPool(device,DescriptorPool,nullptr);
        if (DescriptorLayout) vkDestroyDescriptorSetLayout(device,DescriptorLayout,nullptr);
        if (Sampler) vkDestroySampler(device,Sampler,nullptr);
        if (Framebuffer) vkDestroyFramebuffer(device,Framebuffer,nullptr);
        if (WorldPass) vkDestroyRenderPass(device,WorldPass,nullptr);
        if (ColorView) vkDestroyImageView(device,ColorView,nullptr);
        if (DepthView) vkDestroyImageView(device,DepthView,nullptr);
        if (Color) vkDestroyImage(device,Color,nullptr);
        if (Depth) vkDestroyImage(device,Depth,nullptr);
        if (ColorMemory) vkFreeMemory(device,ColorMemory,nullptr);
        if (DepthMemory) vkFreeMemory(device,DepthMemory,nullptr);
    }
}
