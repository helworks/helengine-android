#include "FakeTrimApi.hpp"
#include "android/looper.h"
#include "../../src/platform/android/rendering/AndroidSceneGpu.hpp"
#include <stdexcept>

namespace {
    /// Mirrors object identity for synthetic Java references without creating a desktop VM.
    jboolean JNICALL IsSameObject(JNIEnv*,jobject left,jobject right) { return left==right?JNI_TRUE:JNI_FALSE; }
    /// Supplies the narrow JNI boundary used by the production registry's callback lookup.
    JNINativeInterface_ CreateJniTable() { JNINativeInterface_ table{}; table.IsSameObject=IsSameObject; return table; }
    /// JNI table outlives all simulated activity registrations.
    const JNINativeInterface_ JniTable=CreateJniTable();
}
JNIEnv FakeTrimApi::Environment{&JniTable};
bool FakeTrimApi::DeviceInitialized=true,FakeTrimApi::WaitFails=false;
std::vector<std::string> FakeTrimApi::Calls;
void FakeTrimApi::Reset() { DeviceInitialized=true; WaitFails=false; Calls.clear(); }
void ALooper_acquire(ALooper* looper) { ++looper->References; }
void ALooper_release(ALooper* looper) { --looper->References; }
void ALooper_wake(ALooper* looper) { ++looper->Wakes; }

namespace helengine::android {
    // Device, surface and shadow bootstrapping are fake driver boundaries; all trimming methods remain production code.
    AndroidVulkanDevice::AndroidVulkanDevice() { if (FakeTrimApi::DeviceInitialized) Device=reinterpret_cast<VkDevice>(1); }
    AndroidVulkanDevice::~AndroidVulkanDevice() {}
    void AndroidVulkanDevice::WaitIdle() const { FakeTrimApi::Calls.push_back("wait"); if (FakeTrimApi::WaitFails) throw std::runtime_error("Injected device loss."); }
    AndroidShadowMap::~AndroidShadowMap() {}
    AndroidSceneGpu::AndroidSceneGpu(AndroidVulkanDevice& device) : Device(device),Shadows(device) { White=std::make_shared<SceneTexture>(); }
    AndroidSceneGpu::~AndroidSceneGpu() {}
}
/// Observes retirement of cached image descriptors after the device wait succeeds.
VKAPI_ATTR VkResult VKAPI_CALL vkFreeDescriptorSets(VkDevice,VkDescriptorPool,uint32_t,const VkDescriptorSet*) { FakeTrimApi::Calls.push_back("descriptor"); return VK_SUCCESS; }
/// Observes image-view destruction order.
VKAPI_ATTR void VKAPI_CALL vkDestroyImageView(VkDevice,VkImageView,const VkAllocationCallbacks*) { FakeTrimApi::Calls.push_back("view"); }
/// Observes image destruction before freeing its backing memory.
VKAPI_ATTR void VKAPI_CALL vkDestroyImage(VkDevice,VkImage,const VkAllocationCallbacks*) { FakeTrimApi::Calls.push_back("image"); }
/// Observes native memory retirement for images and frame arenas.
VKAPI_ATTR void VKAPI_CALL vkFreeMemory(VkDevice,VkDeviceMemory,const VkAllocationCallbacks*) { FakeTrimApi::Calls.push_back("free"); }
/// Observes coherent buffer unmapping before buffer destruction and memory release.
VKAPI_ATTR void VKAPI_CALL vkUnmapMemory(VkDevice,VkDeviceMemory) { FakeTrimApi::Calls.push_back("unmap"); }
/// Observes frame arena buffer destruction before its memory is released.
VKAPI_ATTR void VKAPI_CALL vkDestroyBuffer(VkDevice,VkBuffer,const VkAllocationCallbacks*) { FakeTrimApi::Calls.push_back("buffer"); }
