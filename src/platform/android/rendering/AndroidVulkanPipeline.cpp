#include "AndroidVulkanPipeline.hpp"
#include "vulkan_check.hpp"

#include <memory>
#include <vector>

namespace helengine::android {
    AndroidVulkanPipeline::AndroidVulkanPipeline(AndroidVulkanDevice& device) : Device(device) {}
    AndroidVulkanPipeline::~AndroidVulkanPipeline() { Reset(); }

    VkShaderModule AndroidVulkanPipeline::LoadShader(AAssetManager* assets, const char* path) {
        std::unique_ptr<AAsset, decltype(&AAsset_close)> asset(AAssetManager_open(assets, path, AASSET_MODE_BUFFER), &AAsset_close);
        if (!asset) throw std::runtime_error(std::string("Missing packaged Vulkan shader: ") + path);
        const auto length = AAsset_getLength(asset.get());
        if (length < 20 || length % 4 != 0) throw std::runtime_error(std::string("Invalid SPIR-V asset length: ") + path);
        std::vector<std::uint32_t> words(static_cast<std::size_t>(length) / 4);
        std::size_t read = 0;
        while (read < static_cast<std::size_t>(length)) {
            const int count = AAsset_read(asset.get(), reinterpret_cast<char*>(words.data()) + read, static_cast<std::size_t>(length) - read);
            if (count <= 0) throw std::runtime_error(std::string("Incomplete packaged SPIR-V asset: ") + path);
            read += static_cast<std::size_t>(count);
        }
        if (words[0] != 0x07230203) throw std::runtime_error(std::string("Invalid SPIR-V header: ") + path);
        VkShaderModuleCreateInfo shader{};
        shader.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shader.codeSize = static_cast<std::size_t>(length);
        shader.pCode = words.data();
        VkShaderModule module = VK_NULL_HANDLE;
        check_vulkan(vkCreateShaderModule(Device.GetDevice(), &shader, nullptr, &module), "vkCreateShaderModule");
        return module;
    }

    void AndroidVulkanPipeline::Initialize(AAssetManager* assets, const AndroidVulkanSwapchain& swapchain) {
        if (Pipeline != VK_NULL_HANDLE) throw std::logic_error("Release the previous Vulkan pipeline before recreating it.");
        VertexShader = LoadShader(assets, "shaders/boot.vert.spv");
        FragmentShader = LoadShader(assets, "shaders/boot.frag.spv");
        VkPipelineLayoutCreateInfo layout{};
        layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        check_vulkan(vkCreatePipelineLayout(Device.GetDevice(), &layout, nullptr, &Layout), "vkCreatePipelineLayout");
        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = VertexShader;
        stages[0].pName = "main";
        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = FragmentShader;
        stages[1].pName = "main";
        VkPipelineVertexInputStateCreateInfo vertices{};
        vertices.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        VkPipelineInputAssemblyStateCreateInfo assembly{};
        assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        const VkExtent2D extent = swapchain.GetExtent();
        VkViewport viewport{0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.0f, 1.0f};
        VkRect2D scissor{{0, 0}, extent};
        VkPipelineViewportStateCreateInfo viewports{};
        viewports.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewports.viewportCount = 1;
        viewports.pViewports = &viewport;
        viewports.scissorCount = 1;
        viewports.pScissors = &scissor;
        VkPipelineRasterizationStateCreateInfo raster{};
        raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode = VK_CULL_MODE_NONE;
        raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        raster.lineWidth = 1.0f;
        VkPipelineMultisampleStateCreateInfo samples{};
        samples.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        samples.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendAttachmentState color{};
        color.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo blend{};
        blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blend.attachmentCount = 1;
        blend.pAttachments = &color;
        VkGraphicsPipelineCreateInfo pipeline{};
        pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline.stageCount = 2;
        pipeline.pStages = stages;
        pipeline.pVertexInputState = &vertices;
        pipeline.pInputAssemblyState = &assembly;
        pipeline.pViewportState = &viewports;
        pipeline.pRasterizationState = &raster;
        pipeline.pMultisampleState = &samples;
        pipeline.pColorBlendState = &blend;
        pipeline.layout = Layout;
        pipeline.renderPass = swapchain.GetRenderPass();
        check_vulkan(vkCreateGraphicsPipelines(Device.GetDevice(), VK_NULL_HANDLE, 1, &pipeline, nullptr, &Pipeline), "vkCreateGraphicsPipelines");
    }

    void AndroidVulkanPipeline::Reset() {
        const VkDevice device = Device.GetDevice();
        if (Pipeline != VK_NULL_HANDLE) vkDestroyPipeline(device, Pipeline, nullptr);
        if (Layout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, Layout, nullptr);
        if (VertexShader != VK_NULL_HANDLE) vkDestroyShaderModule(device, VertexShader, nullptr);
        if (FragmentShader != VK_NULL_HANDLE) vkDestroyShaderModule(device, FragmentShader, nullptr);
        Pipeline = VK_NULL_HANDLE;
        Layout = VK_NULL_HANDLE;
        VertexShader = VK_NULL_HANDLE;
        FragmentShader = VK_NULL_HANDLE;
    }
}
