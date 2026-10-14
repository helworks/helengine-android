#include "AndroidFramePacer.hpp"
#include "FrameRateMath.hpp"
#include <swappy/swappyVk.h>
#include <android/log.h>
#include <vector>

namespace helengine::android {
    AndroidFramePacer::AndroidFramePacer(ANativeActivity* activity, AndroidVulkanDevice& device) : Activity(activity), Device(device) {
        if (!Activity || !Activity->vm || !Activity->clazz) throw std::invalid_argument("Android frame pacing requires a live native activity.");
        const int status=Activity->vm->GetEnv(reinterpret_cast<void**>(&Environment),JNI_VERSION_1_6);
        AttachedThread=status==JNI_EDETACHED;
        if ((AttachedThread && Activity->vm->AttachCurrentThread(&Environment,nullptr)!=JNI_OK) || (!AttachedThread && status!=JNI_OK)) {
            throw std::runtime_error("Could not attach the Android frame-pacing thread to the Java VM.");
        }
    }
    AndroidFramePacer::~AndroidFramePacer() {
        Reset();
        if (Device.IsInitialized()) SwappyVk_destroyDevice(Device.GetDevice());
        if (AttachedThread) Activity->vm->DetachCurrentThread();
    }
    void AndroidFramePacer::Initialize(VkSwapchainKHR swapchain, ANativeWindow* window) {
        if (Swapchain!=VK_NULL_HANDLE || swapchain==VK_NULL_HANDLE || !window) throw std::logic_error("Android frame pacing requires a new valid swapchain and window.");
        std::uint64_t refreshDuration=0;
        const bool initialized=SwappyVk_initAndGetRefreshCycleDuration(Environment,Activity->clazz,Device.GetPhysicalDevice(),Device.GetDevice(),swapchain,&refreshDuration);
        if (Environment->ExceptionCheck()) {
            Environment->ExceptionClear();
            SwappyVk_destroySwapchain(Device.GetDevice(),swapchain);
            throw std::runtime_error("Android frame-pacing initialization raised a Java exception.");
        }
        if (!initialized || refreshDuration==0) {
            SwappyVk_destroySwapchain(Device.GetDevice(),swapchain);
            throw std::runtime_error("Android frame pacing could not read the display refresh period.");
        }
        Swapchain=swapchain;
        DisplayPeriod=refreshDuration;
        const int periodCount=SwappyVk_getSupportedRefreshPeriodsNS(nullptr,0,Swapchain);
        if (periodCount>0) {
            std::vector<std::uint64_t> periods(static_cast<std::size_t>(periodCount));
            SwappyVk_getSupportedRefreshPeriodsNS(periods.data(),periodCount,Swapchain);
            for (const auto period:periods) if (period>0 && period<DisplayPeriod) DisplayPeriod=period;
        }
        SwappyVk_setWindow(Device.GetDevice(),Swapchain,window);
        // The engine user owns the FPS choice. Swappy may optimize pipelining without changing that choice.
        SwappyVk_setAutoSwapInterval(false);
        SwappyVk_setSwapIntervalNS(Device.GetDevice(),Swapchain,FrameRateMath::IntervalNanoseconds(TargetFrameRate,DisplayPeriod));
        __android_log_print(ANDROID_LOG_INFO,"HelengineAndroid","Android frame pacing initialized: target=%.0f FPS, display period=%llu ns.",
            TargetFrameRate,static_cast<unsigned long long>(refreshDuration));
    }
    void AndroidFramePacer::Reset() {
        if (Swapchain!=VK_NULL_HANDLE) SwappyVk_destroySwapchain(Device.GetDevice(),Swapchain);
        Swapchain=VK_NULL_HANDLE;
        DisplayPeriod=0;
    }
    void AndroidFramePacer::SetTargetFrameRate(double framesPerSecond) {
        FrameRateMath::Validate(framesPerSecond);
        if (TargetFrameRate==framesPerSecond) return;
        if (Swapchain!=VK_NULL_HANDLE) {
            SwappyVk_setSwapIntervalNS(Device.GetDevice(),Swapchain,FrameRateMath::IntervalNanoseconds(framesPerSecond,DisplayPeriod));
            SwappyVk_resetFramePacing(Swapchain);
        }
        TargetFrameRate=framesPerSecond;
        __android_log_print(ANDROID_LOG_INFO,"HelengineAndroid","Android target frame rate applied: %.0f FPS (0=display rate).",framesPerSecond);
    }
    void AndroidFramePacer::ResetTiming() {
        if (Swapchain!=VK_NULL_HANDLE) SwappyVk_resetFramePacing(Swapchain);
    }
    VkResult AndroidFramePacer::Present(const VkPresentInfoKHR& present) {
        if (Swapchain==VK_NULL_HANDLE) throw std::logic_error("Cannot present without an initialized Android frame pacer.");
        return SwappyVk_queuePresent(Device.GetQueue(),&present);
    }
}
