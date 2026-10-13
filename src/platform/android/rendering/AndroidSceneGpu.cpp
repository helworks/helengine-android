#include "AndroidSceneGpu.hpp"
#include "vulkan_memory.hpp"
#include <cstring>
#include <algorithm>

namespace helengine::android {
    AndroidSceneGpu::AndroidSceneGpu(AndroidVulkanDevice& device) : Device(device) {
        White = std::make_shared<SceneTexture>();
        White->Width = White->Height = 1;
        White->Pixels = {255, 255, 255, 255};
    }
    AndroidSceneGpu::~AndroidSceneGpu() {
        if (!Device.IsInitialized()) return;
        vkDeviceWaitIdle(Device.GetDevice());
        ResetPipelines();
        for (auto& entry : Textures) DestroyTexture(entry.second);
        if (VertexMapping) vkUnmapMemory(Device.GetDevice(), VertexMemory);
        if (VertexBuffer) vkDestroyBuffer(Device.GetDevice(), VertexBuffer, nullptr);
        if (VertexMemory) vkFreeMemory(Device.GetDevice(), VertexMemory, nullptr);
        if (Sampler) vkDestroySampler(Device.GetDevice(), Sampler, nullptr);
        if (UploadPool) vkDestroyCommandPool(Device.GetDevice(), UploadPool, nullptr);
        if (DescriptorPool) vkDestroyDescriptorPool(Device.GetDevice(), DescriptorPool, nullptr);
        if (DescriptorLayout) vkDestroyDescriptorSetLayout(Device.GetDevice(), DescriptorLayout, nullptr);
    }
    VkShaderModule AndroidSceneGpu::LoadShader(AAssetManager* assets, const char* path) {
        std::unique_ptr<AAsset, decltype(&AAsset_close)> asset(AAssetManager_open(assets, path, AASSET_MODE_BUFFER), AAsset_close);
        if (!asset) throw std::runtime_error(std::string("Missing scene shader: ") + path);
        auto size = AAsset_getLength(asset.get());
        if (size < 20 || size % 4) throw std::runtime_error("Invalid scene SPIR-V size.");
        std::vector<std::uint32_t> words(size / 4);
        if (AAsset_read(asset.get(), words.data(), size) != size || words[0] != 0x07230203) throw std::runtime_error("Invalid scene SPIR-V data.");
        VkShaderModuleCreateInfo create{};
        create.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        create.codeSize = size;
        create.pCode = words.data();
        VkShaderModule shader = VK_NULL_HANDLE;
        check_vulkan(vkCreateShaderModule(Device.GetDevice(), &create, nullptr, &shader), "vkCreateShaderModule(scene)");
        return shader;
    }
    void AndroidSceneGpu::Initialize(AAssetManager* assets, const AndroidVulkanSwapchain& swapchain) {
        const auto device = Device.GetDevice();
        if (!DescriptorLayout) {
            VkDescriptorSetLayoutBinding binding{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
            VkDescriptorSetLayoutCreateInfo descriptor{};
            descriptor.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            descriptor.bindingCount = 1;
            descriptor.pBindings = &binding;
            check_vulkan(vkCreateDescriptorSetLayout(device, &descriptor, nullptr, &DescriptorLayout), "vkCreateDescriptorSetLayout");
            VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024};
            VkDescriptorPoolCreateInfo pool{};
            pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            pool.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
            pool.maxSets = 1024;
            pool.poolSizeCount = 1;
            pool.pPoolSizes = &size;
            check_vulkan(vkCreateDescriptorPool(device, &pool, nullptr, &DescriptorPool), "vkCreateDescriptorPool");
            VkSamplerCreateInfo sampler{};
            sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            sampler.magFilter = sampler.minFilter = VK_FILTER_LINEAR;
            sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            sampler.addressModeU = sampler.addressModeV = sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            check_vulkan(vkCreateSampler(device, &sampler, nullptr, &Sampler), "vkCreateSampler");
            VkCommandPoolCreateInfo upload{};
            upload.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            upload.queueFamilyIndex = Device.GetQueueFamily();
            upload.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
            check_vulkan(vkCreateCommandPool(device, &upload, nullptr, &UploadPool), "vkCreateCommandPool(upload)");
        }
        VkPipelineLayoutCreateInfo layout{};
        layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout.setLayoutCount = 1;
        layout.pSetLayouts = &DescriptorLayout;
        check_vulkan(vkCreatePipelineLayout(device, &layout, nullptr, &Layout), "vkCreatePipelineLayout(scene)");
        VkShaderModule vertex = LoadShader(assets, "shaders/scene.vert.spv");
        VkShaderModule fragment = VK_NULL_HANDLE;
        try {
            fragment = LoadShader(assets, "shaders/scene.frag.spv");
            VkPipelineShaderStageCreateInfo stages[2]{};
            for (auto& stage : stages) { stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stage.pName = "main"; }
            stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT; stages[0].module = vertex;
            stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module = fragment;
            VkVertexInputBindingDescription binding{0, sizeof(SceneVertex), VK_VERTEX_INPUT_RATE_VERTEX};
            VkVertexInputAttributeDescription attributes[3]{{0,0,VK_FORMAT_R32G32B32A32_SFLOAT,offsetof(SceneVertex,Position)},
                {1,0,VK_FORMAT_R32G32B32A32_SFLOAT,offsetof(SceneVertex,Color)}, {2,0,VK_FORMAT_R32G32_SFLOAT,offsetof(SceneVertex,TexCoord)}};
            VkPipelineVertexInputStateCreateInfo vertices{};
            vertices.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertices.vertexBindingDescriptionCount = 1; vertices.pVertexBindingDescriptions = &binding;
            vertices.vertexAttributeDescriptionCount = 3; vertices.pVertexAttributeDescriptions = attributes;
            VkPipelineInputAssemblyStateCreateInfo assembly{};
            assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            VkPipelineViewportStateCreateInfo viewport{};
            viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewport.viewportCount = viewport.scissorCount = 1;
            VkDynamicState states[]{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
            VkPipelineDynamicStateCreateInfo dynamic{};
            dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            dynamic.dynamicStateCount = 2; dynamic.pDynamicStates = states;
            VkPipelineRasterizationStateCreateInfo raster{};
            raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            raster.polygonMode = VK_POLYGON_MODE_FILL; raster.lineWidth = 1;
            raster.cullMode = VK_CULL_MODE_NONE;
            VkPipelineMultisampleStateCreateInfo samples{};
            samples.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            samples.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
            VkPipelineDepthStencilStateCreateInfo depth{};
            depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            depth.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
            VkPipelineColorBlendAttachmentState color{};
            color.colorWriteMask = 15;
            color.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            color.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            color.colorBlendOp = VK_BLEND_OP_ADD;
            color.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            color.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            color.alphaBlendOp = VK_BLEND_OP_ADD;
            VkPipelineColorBlendStateCreateInfo blend{};
            blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            blend.attachmentCount = 1; blend.pAttachments = &color;
            VkGraphicsPipelineCreateInfo pipeline{};
            pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            pipeline.stageCount = 2; pipeline.pStages = stages;
            pipeline.pVertexInputState = &vertices; pipeline.pInputAssemblyState = &assembly;
            pipeline.pViewportState = &viewport; pipeline.pDynamicState = &dynamic;
            pipeline.pRasterizationState = &raster; pipeline.pMultisampleState = &samples;
            pipeline.pDepthStencilState = &depth; pipeline.pColorBlendState = &blend;
            pipeline.layout = Layout; pipeline.renderPass = swapchain.GetRenderPass();
            depth.depthTestEnable = depth.depthWriteEnable = VK_TRUE;
            check_vulkan(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline, nullptr, &MeshPipeline), "vkCreateGraphicsPipelines(mesh)");
            depth.depthTestEnable = depth.depthWriteEnable = VK_FALSE;
            color.blendEnable = VK_TRUE;
            check_vulkan(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline, nullptr, &OverlayPipeline), "vkCreateGraphicsPipelines(overlay)");
        } catch (...) {
            if (fragment) vkDestroyShaderModule(device, fragment, nullptr);
            vkDestroyShaderModule(device, vertex, nullptr);
            throw;
        }
        vkDestroyShaderModule(device, fragment, nullptr);
        vkDestroyShaderModule(device, vertex, nullptr);
    }
    void AndroidSceneGpu::ResetPipelines() {
        const auto device = Device.GetDevice();
        if (MeshPipeline) vkDestroyPipeline(device, MeshPipeline, nullptr);
        if (OverlayPipeline) vkDestroyPipeline(device, OverlayPipeline, nullptr);
        if (Layout) vkDestroyPipelineLayout(device, Layout, nullptr);
        MeshPipeline = OverlayPipeline = VK_NULL_HANDLE; Layout = VK_NULL_HANDLE;
    }
    void AndroidSceneGpu::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer& buffer, VkDeviceMemory& memory, void*& mapped) {
        VkBufferCreateInfo create{};
        create.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        create.size = size; create.usage = usage;
        check_vulkan(vkCreateBuffer(Device.GetDevice(), &create, nullptr, &buffer), "vkCreateBuffer");
        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(Device.GetDevice(), buffer, &requirements);
        VkMemoryAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate.allocationSize = requirements.size;
        allocate.memoryTypeIndex = FindMemoryType(Device, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        check_vulkan(vkAllocateMemory(Device.GetDevice(), &allocate, nullptr, &memory), "vkAllocateMemory(buffer)");
        check_vulkan(vkBindBufferMemory(Device.GetDevice(), buffer, memory, 0), "vkBindBufferMemory");
        check_vulkan(vkMapMemory(Device.GetDevice(), memory, 0, size, 0, &mapped), "vkMapMemory");
    }
    void AndroidSceneGpu::DestroyTexture(TextureResource& texture) {
        const auto device = Device.GetDevice();
        if (texture.Descriptor) vkFreeDescriptorSets(device, DescriptorPool, 1, &texture.Descriptor);
        if (texture.View) vkDestroyImageView(device, texture.View, nullptr);
        if (texture.Image) vkDestroyImage(device, texture.Image, nullptr);
        if (texture.Memory) vkFreeMemory(device, texture.Memory, nullptr);
        texture = {};
    }
    void AndroidSceneGpu::UploadTexture(const std::shared_ptr<SceneTexture>& source, TextureResource& texture) {
        Device.WaitIdle();
        DestroyTexture(texture);
        if (!source->Width || !source->Height || source->Pixels.size() != static_cast<std::size_t>(source->Width) * source->Height * 4) {
            throw std::runtime_error("Scene textures require complete RGBA8 image data.");
        }
        texture.Source = source; texture.Revision = source->Revision;
        const auto device = Device.GetDevice();
        VkImageCreateInfo create{};
        create.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        create.imageType = VK_IMAGE_TYPE_2D; create.format = VK_FORMAT_R8G8B8A8_UNORM;
        create.extent = {source->Width, source->Height, 1}; create.mipLevels = create.arrayLayers = 1;
        create.samples = VK_SAMPLE_COUNT_1_BIT; create.tiling = VK_IMAGE_TILING_OPTIMAL;
        create.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        check_vulkan(vkCreateImage(device, &create, nullptr, &texture.Image), "vkCreateImage(texture)");
        texture.Memory = AllocateImageMemory(Device, texture.Image);
        VkBuffer staging = VK_NULL_HANDLE; VkDeviceMemory memory = VK_NULL_HANDLE; void* mapping = nullptr;
        CreateBuffer(source->Pixels.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, staging, memory, mapping);
        std::memcpy(mapping, source->Pixels.data(), source->Pixels.size());
        vkUnmapMemory(device, memory);
        VkCommandBufferAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocate.commandPool = UploadPool; allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; allocate.commandBufferCount = 1;
        VkCommandBuffer command = VK_NULL_HANDLE;
        check_vulkan(vkAllocateCommandBuffers(device, &allocate, &command), "vkAllocateCommandBuffers(upload)");
        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        check_vulkan(vkBeginCommandBuffer(command, &begin), "vkBeginCommandBuffer(upload)");
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = texture.Image; barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,nullptr,0,nullptr,1,&barrier);
        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}; copy.imageExtent = create.extent;
        vkCmdCopyBufferToImage(command, staging, texture.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
        check_vulkan(vkEndCommandBuffer(command), "vkEndCommandBuffer(upload)");
        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO; submit.commandBufferCount = 1; submit.pCommandBuffers = &command;
        check_vulkan(vkQueueSubmit(Device.GetQueue(),1,&submit,VK_NULL_HANDLE), "vkQueueSubmit(upload)");
        Device.WaitIdle();
        vkFreeCommandBuffers(device,UploadPool,1,&command);
        vkDestroyBuffer(device,staging,nullptr); vkFreeMemory(device,memory,nullptr);
        VkImageViewCreateInfo view{};
        view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO; view.image = texture.Image;
        view.viewType = VK_IMAGE_VIEW_TYPE_2D; view.format = create.format; view.subresourceRange = barrier.subresourceRange;
        check_vulkan(vkCreateImageView(device,&view,nullptr,&texture.View), "vkCreateImageView(texture)");
        VkDescriptorSetAllocateInfo descriptors{};
        descriptors.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptors.descriptorPool = DescriptorPool; descriptors.descriptorSetCount = 1; descriptors.pSetLayouts = &DescriptorLayout;
        check_vulkan(vkAllocateDescriptorSets(device,&descriptors,&texture.Descriptor), "vkAllocateDescriptorSets");
        VkDescriptorImageInfo image{Sampler, texture.View, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; write.dstSet = texture.Descriptor;
        write.descriptorCount = 1; write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; write.pImageInfo = &image;
        vkUpdateDescriptorSets(device,1,&write,0,nullptr);
    }
    void AndroidSceneGpu::Prepare(const std::vector<SceneBatch>& batches) {
        for (auto it = Textures.begin(); it != Textures.end();) {
            if (it->second.Source.expired()) { DestroyTexture(it->second); it = Textures.erase(it); }
            else ++it;
        }
        std::size_t count = 0;
        for (const auto& batch : batches) {
            count += batch.Vertices.size();
            const auto& source = batch.Texture ? batch.Texture : White;
            auto& resource = Textures[source.get()];
            if (resource.Revision != source->Revision) UploadTexture(source, resource);
        }
        const VkDeviceSize bytes = count * sizeof(SceneVertex);
        if (!bytes) return;
        if (bytes > VertexCapacity) {
            if (VertexMapping) vkUnmapMemory(Device.GetDevice(),VertexMemory);
            if (VertexBuffer) vkDestroyBuffer(Device.GetDevice(),VertexBuffer,nullptr);
            if (VertexMemory) vkFreeMemory(Device.GetDevice(),VertexMemory,nullptr);
            VertexCapacity = std::max<VkDeviceSize>(bytes * 2, 65536);
            CreateBuffer(VertexCapacity,VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,VertexBuffer,VertexMemory,VertexMapping);
        }
        auto* destination = static_cast<SceneVertex*>(VertexMapping);
        for (const auto& batch : batches) {
            if (!batch.Vertices.empty()) std::memcpy(destination,batch.Vertices.data(),batch.Vertices.size()*sizeof(SceneVertex));
            destination += batch.Vertices.size();
        }
    }
    void AndroidSceneGpu::Record(VkCommandBuffer command, const std::vector<SceneBatch>& batches) {
        if (!VertexBuffer) return;
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(command,0,1,&VertexBuffer,&offset);
        std::uint32_t first = 0;
        for (const auto& batch : batches) {
            if (batch.Vertices.empty()) continue;
            auto& texture = Textures.at((batch.Texture ? batch.Texture : White).get());
            vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,batch.DepthTest ? MeshPipeline : OverlayPipeline);
            vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_GRAPHICS,Layout,0,1,&texture.Descriptor,0,nullptr);
            VkViewport viewport{static_cast<float>(batch.Viewport.offset.x),static_cast<float>(batch.Viewport.offset.y),
                static_cast<float>(batch.Viewport.extent.width),static_cast<float>(batch.Viewport.extent.height),0,1};
            vkCmdSetViewport(command,0,1,&viewport); vkCmdSetScissor(command,0,1,&batch.Viewport);
            vkCmdDraw(command,static_cast<std::uint32_t>(batch.Vertices.size()),1,first,0);
            first += batch.Vertices.size();
        }
    }
}
