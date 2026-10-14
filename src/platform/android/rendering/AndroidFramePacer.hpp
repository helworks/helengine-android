#pragma once
#include "AndroidVulkanDevice.hpp"
#include <android/native_activity.h>

namespace helengine::android {
    /// Owns Swappy registration and the render thread's JNI attachment across native surface replacements.
    class AndroidFramePacer {
    public:
        /// Borrows the activity and device until renderer destruction on the same native thread.
        AndroidFramePacer(ANativeActivity* activity, AndroidVulkanDevice& device);
        /// Releases Swappy device resources before detaching this class's JNI attachment.
        ~AndroidFramePacer();
        /// Prevents duplicating JNI attachment and swapchain ownership.
        AndroidFramePacer(const AndroidFramePacer&) = delete;
        /// Prevents replacing borrowed device ownership.
        AndroidFramePacer& operator=(const AndroidFramePacer&) = delete;
        /// Registers a newly created swapchain and its live Android window before presentation.
        void Initialize(VkSwapchainKHR swapchain, ANativeWindow* window);
        /// Unregisters the current swapchain before its Vulkan handle is destroyed; retains the requested FPS.
        void Reset();
        /// Changes the explicit requested rate; zero follows available display refresh periods.
        void SetTargetFrameRate(double framesPerSecond);
        /// Discards pre-suspension pacing history before gameplay resumes.
        void ResetTiming();
        /// Presents through Swappy, preserving Vulkan's out-of-date and device-loss results.
        VkResult Present(const VkPresentInfoKHR& present);
    private:
        /// Activity remains alive throughout all renderer callbacks and teardown.
        ANativeActivity* Activity;
        /// Device owns the queue and outlives Swappy resource teardown.
        AndroidVulkanDevice& Device;
        /// Thread-local JNI environment kept attached until the final Swappy cleanup.
        JNIEnv* Environment=nullptr;
        /// True only when this class attached an otherwise detached native render thread.
        bool AttachedThread=false;
        /// Registered swapchain handle, released before AndroidVulkanSwapchain::Reset.
        VkSwapchainKHR Swapchain=VK_NULL_HANDLE;
        /// Engine-requested value persists across swapchain recreation and native window replacement.
        double TargetFrameRate=0;
        /// Shortest supported display period reported for the attached surface; zero while no surface is registered.
        std::uint64_t DisplayPeriod=0;
    };
}
