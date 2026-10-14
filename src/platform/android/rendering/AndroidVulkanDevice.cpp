#include "AndroidVulkanDevice.hpp"
#include "vulkan_check.hpp"

#include <android/log.h>
#include <cstring>
#include <vector>
#include <array>
#include <swappy/swappyVk.h>

namespace helengine::android {
    AndroidVulkanDevice::AndroidVulkanDevice() {
        const char* extensions[] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME};
        VkApplicationInfo application{};
        application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        application.pApplicationName = "HelEngine Android Host";
        application.applicationVersion = VK_MAKE_VERSION(0, 2, 0);
        application.pEngineName = "HelEngine";
        application.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        application.apiVersion = VK_API_VERSION_1_0;
        VkInstanceCreateInfo instance{};
        instance.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instance.pApplicationInfo = &application;
        instance.enabledExtensionCount = 2;
        instance.ppEnabledExtensionNames = extensions;
        check_vulkan(vkCreateInstance(&instance, nullptr, &Instance), "vkCreateInstance");
    }

    AndroidVulkanDevice::~AndroidVulkanDevice() {
        if (Device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(Device);
            if (FrameFence != VK_NULL_HANDLE) vkDestroyFence(Device, FrameFence, nullptr);
            if (ImageAvailable != VK_NULL_HANDLE) vkDestroySemaphore(Device, ImageAvailable, nullptr);
            if (CommandPool != VK_NULL_HANDLE) vkDestroyCommandPool(Device, CommandPool, nullptr);
            vkDestroyDevice(Device, nullptr);
        }
        if (Instance != VK_NULL_HANDLE) vkDestroyInstance(Instance, nullptr);
    }

    void AndroidVulkanDevice::Initialize(VkSurfaceKHR surface) {
        if (IsInitialized()) throw std::logic_error("Vulkan device is already initialized.");
        std::uint32_t count = 0;
        check_vulkan(vkEnumeratePhysicalDevices(Instance, &count, nullptr), "vkEnumeratePhysicalDevices");
        std::vector<VkPhysicalDevice> devices(count);
        check_vulkan(vkEnumeratePhysicalDevices(Instance, &count, devices.data()), "vkEnumeratePhysicalDevices");
        for (VkPhysicalDevice candidate : devices) {
            std::uint32_t extension_count = 0;
            check_vulkan(vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extension_count, nullptr), "vkEnumerateDeviceExtensionProperties");
            std::vector<VkExtensionProperties> extensions(extension_count);
            check_vulkan(vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extension_count, extensions.data()), "vkEnumerateDeviceExtensionProperties");
            bool has_swapchain = false;
            for (const auto& extension : extensions) {
                if (std::strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) has_swapchain = true;
            }
            if (!has_swapchain) continue;
            std::uint32_t family_count = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &family_count, nullptr);
            std::vector<VkQueueFamilyProperties> families(family_count);
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &family_count, families.data());
            for (std::uint32_t family = 0; family < family_count; ++family) {
                VkBool32 presents = VK_FALSE;
                check_vulkan(vkGetPhysicalDeviceSurfaceSupportKHR(candidate, family, surface, &presents), "vkGetPhysicalDeviceSurfaceSupportKHR");
                if (families[family].queueCount > 0 && (families[family].queueFlags & VK_QUEUE_GRAPHICS_BIT) && presents) {
                    PhysicalDevice = candidate;
                    QueueFamily = family;
                    break;
                }
            }
            if (PhysicalDevice != VK_NULL_HANDLE) break;
        }
        if (PhysicalDevice == VK_NULL_HANDLE) throw std::runtime_error("No Vulkan device has a graphics queue capable of Android presentation.");

        const float priority = 1.0f;
        VkDeviceQueueCreateInfo queue{};
        queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue.queueFamilyIndex = QueueFamily;
        queue.queueCount = 1;
        queue.pQueuePriorities = &priority;
        std::uint32_t availableCount=0;
        check_vulkan(vkEnumerateDeviceExtensionProperties(PhysicalDevice,nullptr,&availableCount,nullptr),"vkEnumerateDeviceExtensionProperties");
        std::vector<VkExtensionProperties> available(availableCount);
        check_vulkan(vkEnumerateDeviceExtensionProperties(PhysicalDevice,nullptr,&availableCount,available.data()),"vkEnumerateDeviceExtensionProperties");
        std::uint32_t requiredCount=0;
        SwappyVk_determineDeviceExtensions(PhysicalDevice,availableCount,available.data(),&requiredCount,nullptr);
        std::vector<std::array<char,VK_MAX_EXTENSION_NAME_SIZE+1>> extensionNames(requiredCount);
        std::vector<char*> requiredNames;
        for (auto& name:extensionNames) requiredNames.push_back(name.data());
        if (requiredCount>0) SwappyVk_determineDeviceExtensions(PhysicalDevice,availableCount,available.data(),&requiredCount,requiredNames.data());
        std::vector<const char*> enabledNames{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        for (std::uint32_t index=0;index<requiredCount;++index) {
            if (std::strcmp(requiredNames[index],VK_KHR_SWAPCHAIN_EXTENSION_NAME)!=0) enabledNames.push_back(requiredNames[index]);
        }
        VkDeviceCreateInfo device{};
        device.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device.queueCreateInfoCount = 1;
        device.pQueueCreateInfos = &queue;
        device.enabledExtensionCount = static_cast<std::uint32_t>(enabledNames.size());
        device.ppEnabledExtensionNames = enabledNames.data();
        check_vulkan(vkCreateDevice(PhysicalDevice, &device, nullptr, &Device), "vkCreateDevice");
        vkGetDeviceQueue(Device, QueueFamily, 0, &Queue);
        SwappyVk_setQueueFamilyIndex(Device,Queue,QueueFamily);
        VkCommandPoolCreateInfo pool{};
        pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool.queueFamilyIndex = QueueFamily;
        check_vulkan(vkCreateCommandPool(Device, &pool, nullptr, &CommandPool), "vkCreateCommandPool");
        VkCommandBufferAllocateInfo command{};
        command.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        command.commandPool = CommandPool;
        command.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        command.commandBufferCount = 1;
        check_vulkan(vkAllocateCommandBuffers(Device, &command, &CommandBuffer), "vkAllocateCommandBuffers");
        VkSemaphoreCreateInfo semaphore{};
        semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        check_vulkan(vkCreateSemaphore(Device, &semaphore, nullptr, &ImageAvailable), "vkCreateSemaphore");
        VkFenceCreateInfo fence{};
        fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        check_vulkan(vkCreateFence(Device, &fence, nullptr, &FrameFence), "vkCreateFence");
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(PhysicalDevice, &properties);
        __android_log_print(ANDROID_LOG_INFO, "HelengineAndroid", "Vulkan device initialized: %s, API %u.%u.",
            properties.deviceName, VK_VERSION_MAJOR(properties.apiVersion), VK_VERSION_MINOR(properties.apiVersion));
    }

    void AndroidVulkanDevice::WaitIdle() const {
        if (Device != VK_NULL_HANDLE) check_vulkan(vkDeviceWaitIdle(Device), "vkDeviceWaitIdle");
    }
}
