#include "AndroidVulkanSwapchain.hpp"
#include "AndroidRenderPassDependencies.hpp"
#include "vulkan_memory.hpp"
#include "vulkan_check.hpp"

#include <android/log.h>
#include <algorithm>
#include <limits>

namespace helengine::android {
    AndroidVulkanSwapchain::AndroidVulkanSwapchain(AndroidVulkanDevice& device) : Device(device) {}
    AndroidVulkanSwapchain::~AndroidVulkanSwapchain() { Reset(); }

    void AndroidVulkanSwapchain::Initialize(VkSurfaceKHR surface, ANativeWindow* window, VkExtent2D contentExtent) {
        if (Swapchain != VK_NULL_HANDLE) throw std::logic_error("Release the previous Vulkan swapchain before recreating it.");
        VkSurfaceCapabilitiesKHR capabilities{};
        check_vulkan(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Device.GetPhysicalDevice(), surface, &capabilities), "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
        if (!(capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)) {
            throw std::runtime_error("Android surface does not support Vulkan color attachments.");
        }
        std::uint32_t format_count = 0;
        check_vulkan(vkGetPhysicalDeviceSurfaceFormatsKHR(Device.GetPhysicalDevice(), surface, &format_count, nullptr), "vkGetPhysicalDeviceSurfaceFormatsKHR");
        std::vector<VkSurfaceFormatKHR> formats(format_count);
        check_vulkan(vkGetPhysicalDeviceSurfaceFormatsKHR(Device.GetPhysicalDevice(), surface, &format_count, formats.data()), "vkGetPhysicalDeviceSurfaceFormatsKHR");
        VkSurfaceFormatKHR selected{};
        if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
            selected = {VK_FORMAT_B8G8R8A8_UNORM, formats[0].colorSpace};
        } else {
            const VkFormat preferred[] = {VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_R8G8B8A8_SRGB};
            for (VkFormat candidate : preferred) {
                for (const auto& format : formats) {
                    if (format.format == candidate && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                        selected = format;
                        break;
                    }
                }
                if (selected.format != VK_FORMAT_UNDEFINED) break;
            }
        }
        if (selected.format == VK_FORMAT_UNDEFINED) throw std::runtime_error("Android surface has no supported RGBA/BGRA Vulkan color format.");
        Format = selected.format;
        Extent = capabilities.currentExtent;
        if (contentExtent.width > 0 && contentExtent.height > 0) {
            // Android permits a swapchain extent different from currentExtent; the content callback carries the live layout size.
            Extent.width = std::clamp(contentExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            Extent.height = std::clamp(contentExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        } else if (Extent.width == std::numeric_limits<std::uint32_t>::max()) {
            const int width = ANativeWindow_getWidth(window);
            const int height = ANativeWindow_getHeight(window);
            if (width <= 0 || height <= 0) throw std::runtime_error("Android native window has no drawable extent.");
            Extent.width = std::clamp(static_cast<std::uint32_t>(width), capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            Extent.height = std::clamp(static_cast<std::uint32_t>(height), capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        }
        if (Extent.width == 0 || Extent.height == 0) throw std::runtime_error("Vulkan surface has no drawable extent.");
        std::uint32_t image_count = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount > 0) image_count = std::min(image_count, capabilities.maxImageCount);
        VkCompositeAlphaFlagBitsKHR alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        const VkCompositeAlphaFlagBitsKHR alpha_modes[] = {VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR};
        for (auto candidate : alpha_modes) {
            if (capabilities.supportedCompositeAlpha & candidate) { alpha = candidate; break; }
        }
        VkSwapchainCreateInfoKHR swapchain{};
        swapchain.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchain.surface = surface;
        swapchain.minImageCount = image_count;
        swapchain.imageFormat = Format;
        swapchain.imageColorSpace = selected.colorSpace;
        swapchain.imageExtent = Extent;
        swapchain.imageArrayLayers = 1;
        swapchain.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        swapchain.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        // Vertices and image dimensions use Android's logical window orientation. The compositor owns display rotation.
        if (!(capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)) {
            throw std::runtime_error("Android logical-orientation presentation requires the identity surface transform.");
        }
        swapchain.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        swapchain.compositeAlpha = alpha;
        swapchain.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        swapchain.clipped = VK_TRUE;
        check_vulkan(vkCreateSwapchainKHR(Device.GetDevice(), &swapchain, nullptr, &Swapchain), "vkCreateSwapchainKHR");
        check_vulkan(vkGetSwapchainImagesKHR(Device.GetDevice(), Swapchain, &image_count, nullptr), "vkGetSwapchainImagesKHR");
        std::vector<VkImage> images(image_count);
        check_vulkan(vkGetSwapchainImagesKHR(Device.GetDevice(), Swapchain, &image_count, images.data()), "vkGetSwapchainImagesKHR");

        VkAttachmentDescription color{};
        color.format = Format;
        color.samples = VK_SAMPLE_COUNT_1_BIT;
        color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        VkAttachmentReference attachment{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &attachment;
        const auto dependencies=AndroidRenderPassDependencies::Create();
        VkRenderPassCreateInfo render_pass{};
        render_pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass.attachmentCount = 1;
        render_pass.pAttachments = &color;
#if HELENGINE_ANDROID_GAME
        VkFormatProperties depthProperties{};
        vkGetPhysicalDeviceFormatProperties(Device.GetPhysicalDevice(), VK_FORMAT_D32_SFLOAT, &depthProperties);
        if (!(depthProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
            throw std::runtime_error("Android renderer requires D32 depth attachments.");
        }
        VkImageCreateInfo depthImage{};
        depthImage.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        depthImage.imageType = VK_IMAGE_TYPE_2D;
        depthImage.format = VK_FORMAT_D32_SFLOAT;
        depthImage.extent = {Extent.width, Extent.height, 1};
        depthImage.mipLevels = 1;
        depthImage.arrayLayers = 1;
        depthImage.samples = VK_SAMPLE_COUNT_1_BIT;
        depthImage.tiling = VK_IMAGE_TILING_OPTIMAL;
        depthImage.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        check_vulkan(vkCreateImage(Device.GetDevice(), &depthImage, nullptr, &DepthImage), "vkCreateImage(depth)");
        DepthMemory = AllocateImageMemory(Device, DepthImage);
        VkImageViewCreateInfo depthView{};
        depthView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        depthView.image = DepthImage;
        depthView.viewType = VK_IMAGE_VIEW_TYPE_2D;
        depthView.format = depthImage.format;
        depthView.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
        check_vulkan(vkCreateImageView(Device.GetDevice(), &depthView, nullptr, &DepthView), "vkCreateImageView(depth)");
        VkAttachmentDescription attachments[2]{color, {}};
        attachments[1].format = depthImage.format;
        attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        VkAttachmentReference depthAttachment{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        subpass.pDepthStencilAttachment = &depthAttachment;
        render_pass.attachmentCount = 2;
        render_pass.pAttachments = attachments;
#endif
        render_pass.subpassCount = 1;
        render_pass.pSubpasses = &subpass;
        render_pass.dependencyCount = dependencies.size();
        render_pass.pDependencies = dependencies.data();
        check_vulkan(vkCreateRenderPass(Device.GetDevice(), &render_pass, nullptr, &RenderPass), "vkCreateRenderPass");
#if HELENGINE_ANDROID_GAME
        attachments[0].loadOp=VK_ATTACHMENT_LOAD_OP_LOAD;
        attachments[0].initialLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        attachments[1].loadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;
#else
        color.loadOp=VK_ATTACHMENT_LOAD_OP_LOAD;
        color.initialLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
#endif
        check_vulkan(vkCreateRenderPass(Device.GetDevice(),&render_pass,nullptr,&LoadRenderPass),"vkCreateRenderPass(presentation continuation)");

        ImageViews.resize(image_count, VK_NULL_HANDLE);
        Framebuffers.resize(image_count, VK_NULL_HANDLE);
        RenderFinished.resize(image_count, VK_NULL_HANDLE);
        for (std::uint32_t image = 0; image < image_count; ++image) {
            VkImageViewCreateInfo view{};
            view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            view.image = images[image];
            view.viewType = VK_IMAGE_VIEW_TYPE_2D;
            view.format = Format;
            view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            view.subresourceRange.levelCount = 1;
            view.subresourceRange.layerCount = 1;
            check_vulkan(vkCreateImageView(Device.GetDevice(), &view, nullptr, &ImageViews[image]), "vkCreateImageView");
            VkFramebufferCreateInfo framebuffer{};
            framebuffer.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebuffer.renderPass = RenderPass;
            framebuffer.attachmentCount = 1;
            framebuffer.pAttachments = &ImageViews[image];
#if HELENGINE_ANDROID_GAME
            VkImageView framebufferViews[2]{ImageViews[image], DepthView};
            framebuffer.attachmentCount = 2;
            framebuffer.pAttachments = framebufferViews;
#endif
            framebuffer.width = Extent.width;
            framebuffer.height = Extent.height;
            framebuffer.layers = 1;
            check_vulkan(vkCreateFramebuffer(Device.GetDevice(), &framebuffer, nullptr, &Framebuffers[image]), "vkCreateFramebuffer");
            VkSemaphoreCreateInfo semaphore{};
            semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            check_vulkan(vkCreateSemaphore(Device.GetDevice(), &semaphore, nullptr, &RenderFinished[image]), "vkCreateSemaphore");
        }
        __android_log_print(ANDROID_LOG_INFO, "HelengineAndroid", "Vulkan swapchain created: %ux%u, %u images.", Extent.width, Extent.height, image_count);
        __android_log_print(ANDROID_LOG_INFO, "HelengineAndroid", "Vulkan logical window orientation: currentTransform=%u presentationTransform=%u.",
            capabilities.currentTransform, swapchain.preTransform);
    }

    void AndroidVulkanSwapchain::Reset() {
        const VkDevice device = Device.GetDevice();
        for (auto framebuffer : Framebuffers) if (framebuffer != VK_NULL_HANDLE) vkDestroyFramebuffer(device, framebuffer, nullptr);
        for (auto view : ImageViews) if (view != VK_NULL_HANDLE) vkDestroyImageView(device, view, nullptr);
        for (auto semaphore : RenderFinished) if (semaphore != VK_NULL_HANDLE) vkDestroySemaphore(device, semaphore, nullptr);
        if (DepthView != VK_NULL_HANDLE) vkDestroyImageView(device, DepthView, nullptr);
        if (DepthImage != VK_NULL_HANDLE) vkDestroyImage(device, DepthImage, nullptr);
        if (DepthMemory != VK_NULL_HANDLE) vkFreeMemory(device, DepthMemory, nullptr);
        DepthView = VK_NULL_HANDLE;
        DepthImage = VK_NULL_HANDLE;
        DepthMemory = VK_NULL_HANDLE;
        if (RenderPass != VK_NULL_HANDLE) vkDestroyRenderPass(device, RenderPass, nullptr);
        if (LoadRenderPass != VK_NULL_HANDLE) vkDestroyRenderPass(device,LoadRenderPass,nullptr);
        if (Swapchain != VK_NULL_HANDLE) vkDestroySwapchainKHR(device, Swapchain, nullptr);
        Framebuffers.clear();
        ImageViews.clear();
        RenderFinished.clear();
        RenderPass = VK_NULL_HANDLE;
        LoadRenderPass=VK_NULL_HANDLE;
        Swapchain = VK_NULL_HANDLE;
        Format = VK_FORMAT_UNDEFINED;
        Extent = {};
    }
}
