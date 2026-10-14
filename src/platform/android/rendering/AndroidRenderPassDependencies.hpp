#pragma once
#include <vulkan/vulkan.h>
#include <array>

namespace helengine::android {
    /// Supplies identical dependency contracts for compatible presentation and sampled world render passes.
    class AndroidRenderPassDependencies {
    public:
        /// Orders attachment writes, depth reuse and composite sampling across separate passes without framebuffer-local assumptions.
        static std::array<VkSubpassDependency,2> Create() {
            const auto attachments=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            const auto writes=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            const auto accesses=writes|VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
            std::array<VkSubpassDependency,2> dependencies{};
            dependencies[0].srcSubpass=VK_SUBPASS_EXTERNAL;
            dependencies[0].dstSubpass=0;
            dependencies[0].srcStageMask=attachments|VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            dependencies[0].dstStageMask=attachments;
            dependencies[0].srcAccessMask=writes|VK_ACCESS_SHADER_READ_BIT;
            dependencies[0].dstAccessMask=accesses;
            dependencies[1].srcSubpass=0;
            dependencies[1].dstSubpass=VK_SUBPASS_EXTERNAL;
            dependencies[1].srcStageMask=attachments;
            dependencies[1].dstStageMask=attachments|VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            dependencies[1].srcAccessMask=writes;
            dependencies[1].dstAccessMask=accesses|VK_ACCESS_SHADER_READ_BIT;
            return dependencies;
        }
    };
}
