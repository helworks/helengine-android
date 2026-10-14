#include "AndroidShadowMap.hpp"
#include "AndroidSceneGpu.hpp"
#include "vulkan_memory.hpp"

namespace helengine::android {
    AndroidShadowMap::~AndroidShadowMap() {
        if (!Device.IsInitialized()) return;
        Device.WaitIdle(); ResetPipeline(); const auto device=Device.GetDevice();
        if (Framebuffer) vkDestroyFramebuffer(device,Framebuffer,nullptr);
        if (Pass) vkDestroyRenderPass(device,Pass,nullptr);
        if (Sampler) vkDestroySampler(device,Sampler,nullptr);
        if (View) vkDestroyImageView(device,View,nullptr);
        if (Image) vkDestroyImage(device,Image,nullptr);
        if (Memory) vkFreeMemory(device,Memory,nullptr);
    }
    void AndroidShadowMap::ResetPipeline() {
        if (Pipeline) vkDestroyPipeline(Device.GetDevice(),Pipeline,nullptr);
        Pipeline=VK_NULL_HANDLE;
    }
    void AndroidShadowMap::Initialize(VkPipelineLayout layout,VkShaderModule vertex,VkShaderModule fragment) {
        const auto device=Device.GetDevice();
        if (!Image) {
            VkFormatProperties format{}; vkGetPhysicalDeviceFormatProperties(Device.GetPhysicalDevice(),VK_FORMAT_D32_SFLOAT,&format);
            constexpr auto required=VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
            if ((format.optimalTilingFeatures&required)!=required) throw std::runtime_error("Android directional shadows require sampled D32 depth attachments.");
            VkImageCreateInfo image{}; image.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            image.imageType=VK_IMAGE_TYPE_2D; image.format=VK_FORMAT_D32_SFLOAT; image.extent={Size,Size,1};
            image.mipLevels=image.arrayLayers=1; image.samples=VK_SAMPLE_COUNT_1_BIT; image.tiling=VK_IMAGE_TILING_OPTIMAL;
            image.usage=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
            check_vulkan(vkCreateImage(device,&image,nullptr,&Image),"vkCreateImage(shadow)"); Memory=AllocateImageMemory(Device,Image);
            VkImageViewCreateInfo view{}; view.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO; view.image=Image;
            view.viewType=VK_IMAGE_VIEW_TYPE_2D; view.format=image.format; view.subresourceRange={VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,1};
            check_vulkan(vkCreateImageView(device,&view,nullptr,&View),"vkCreateImageView(shadow)");
            VkSamplerCreateInfo sampler{}; sampler.sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            sampler.magFilter=sampler.minFilter=VK_FILTER_NEAREST; sampler.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
            sampler.addressModeU=sampler.addressModeV=sampler.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
            sampler.borderColor=VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE; sampler.compareEnable=VK_TRUE; sampler.compareOp=VK_COMPARE_OP_LESS_OR_EQUAL;
            check_vulkan(vkCreateSampler(device,&sampler,nullptr,&Sampler),"vkCreateSampler(shadow)");
            VkAttachmentDescription attachment{}; attachment.format=image.format; attachment.samples=VK_SAMPLE_COUNT_1_BIT;
            attachment.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR; attachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
            attachment.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE; attachment.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachment.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED; attachment.finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            VkAttachmentReference depth{0,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
            VkSubpassDescription subpass{}; subpass.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS; subpass.pDepthStencilAttachment=&depth;
            VkSubpassDependency dependencies[2]{};
            dependencies[0].srcSubpass=VK_SUBPASS_EXTERNAL; dependencies[0].dstSubpass=0;
            dependencies[0].srcStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; dependencies[0].dstStageMask=VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            dependencies[0].srcAccessMask=VK_ACCESS_SHADER_READ_BIT; dependencies[0].dstAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            dependencies[1].srcSubpass=0; dependencies[1].dstSubpass=VK_SUBPASS_EXTERNAL;
            dependencies[1].srcStageMask=VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT; dependencies[1].dstStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            dependencies[1].srcAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT; dependencies[1].dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
            VkRenderPassCreateInfo pass{}; pass.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            pass.attachmentCount=1; pass.pAttachments=&attachment; pass.subpassCount=1; pass.pSubpasses=&subpass; pass.dependencyCount=2; pass.pDependencies=dependencies;
            check_vulkan(vkCreateRenderPass(device,&pass,nullptr,&Pass),"vkCreateRenderPass(shadow)");
            VkFramebufferCreateInfo framebuffer{}; framebuffer.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO; framebuffer.renderPass=Pass;
            framebuffer.attachmentCount=1; framebuffer.pAttachments=&View; framebuffer.width=framebuffer.height=Size; framebuffer.layers=1;
            check_vulkan(vkCreateFramebuffer(device,&framebuffer,nullptr,&Framebuffer),"vkCreateFramebuffer(shadow)");
        }
        VkPipelineShaderStageCreateInfo stages[2]{};
        for (auto& stage:stages) { stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stage.pName="main"; }
        stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT; stages[0].module=vertex; stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module=fragment;
        VkVertexInputBindingDescription binding{0,sizeof(SceneVertex),VK_VERTEX_INPUT_RATE_VERTEX};
        VkVertexInputAttributeDescription attributes[]{{1,0,VK_FORMAT_R32G32B32A32_SFLOAT,offsetof(SceneVertex,Color)},
            {2,0,VK_FORMAT_R32G32_SFLOAT,offsetof(SceneVertex,TexCoord)},{3,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(SceneVertex,WorldPosition)}};
        VkPipelineVertexInputStateCreateInfo vertices{}; vertices.sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertices.vertexBindingDescriptionCount=1; vertices.pVertexBindingDescriptions=&binding; vertices.vertexAttributeDescriptionCount=3; vertices.pVertexAttributeDescriptions=attributes;
        VkPipelineInputAssemblyStateCreateInfo assembly{}; assembly.sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO; assembly.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo viewport{}; viewport.sType=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO; viewport.viewportCount=viewport.scissorCount=1;
        VkDynamicState dynamicStates[]{VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic{}; dynamic.sType=VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO; dynamic.dynamicStateCount=2; dynamic.pDynamicStates=dynamicStates;
        VkPipelineRasterizationStateCreateInfo raster{}; raster.sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        raster.polygonMode=VK_POLYGON_MODE_FILL; raster.cullMode=VK_CULL_MODE_NONE; raster.lineWidth=1; raster.depthBiasEnable=VK_TRUE;
        raster.depthBiasConstantFactor=1.25f; raster.depthBiasSlopeFactor=1.75f;
        VkPipelineMultisampleStateCreateInfo samples{}; samples.sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO; samples.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
        VkPipelineDepthStencilStateCreateInfo depth{}; depth.sType=VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth.depthTestEnable=depth.depthWriteEnable=VK_TRUE; depth.depthCompareOp=VK_COMPARE_OP_LESS_OR_EQUAL;
        VkPipelineColorBlendStateCreateInfo blend{}; blend.sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        VkGraphicsPipelineCreateInfo pipeline{}; pipeline.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline.stageCount=2; pipeline.pStages=stages; pipeline.pVertexInputState=&vertices; pipeline.pInputAssemblyState=&assembly;
        pipeline.pViewportState=&viewport; pipeline.pRasterizationState=&raster; pipeline.pMultisampleState=&samples;
        pipeline.pDepthStencilState=&depth; pipeline.pColorBlendState=&blend; pipeline.pDynamicState=&dynamic;
        pipeline.layout=layout; pipeline.renderPass=Pass;
        check_vulkan(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&pipeline,nullptr,&Pipeline),"vkCreateGraphicsPipelines(shadow)");
    }
    void AndroidShadowMap::Begin(VkCommandBuffer command) {
        VkClearValue clear{}; clear.depthStencil={1,0};
        VkRenderPassBeginInfo pass{}; pass.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        pass.renderPass=Pass; pass.framebuffer=Framebuffer; pass.renderArea.extent={Size,Size}; pass.clearValueCount=1; pass.pClearValues=&clear;
        vkCmdBeginRenderPass(command,&pass,VK_SUBPASS_CONTENTS_INLINE); vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,Pipeline);
        VkViewport viewport{0,0,static_cast<float>(Size),static_cast<float>(Size),0,1}; VkRect2D scissor{{0,0},{Size,Size}};
        vkCmdSetViewport(command,0,1,&viewport); vkCmdSetScissor(command,0,1,&scissor);
    }
    void AndroidShadowMap::End(VkCommandBuffer command) { vkCmdEndRenderPass(command); }
}
