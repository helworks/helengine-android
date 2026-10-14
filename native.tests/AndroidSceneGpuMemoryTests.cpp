#include "../src/platform/android/rendering/AndroidSceneGpu.hpp"
#include "../src/platform/android/diagnostics/AndroidMemoryTrimBridge.hpp"
#include "trim.fakes/FakeTrimApi.hpp"
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>

/// Real JNI entry point is tested along with direct legacy glue requests.
extern "C" JNIEXPORT void JNICALL Java_com_helengine_android_HelengineActivity_nativeRequestMemoryTrim(JNIEnv*,jobject,jint);

namespace helengine::android {
    /// Exercises actual renderer cleanup and callback delivery against fake driver/bootstrap boundaries.
    class AndroidSceneGpuMemoryTests {
    public:
        /// Runs GPU ownership, lifecycle, notification and concurrency scenarios without an emulator or GPU driver.
        static void Run() {
            RetiresOnlyExpiredTextures(); ReleasesArenas(); RepeatedTrim(); DeviceWaitFailure(); ColdLifecycle();
            ActivityRouting(); CoalescingAndWake(); ConcurrentNotifications(); LateCallbacksAndProbe();
            std::cout<<"9 Android memory-trim scenarios passed.\n";
        }
    private:
        /// Requires an observable cleanup or delivery contract even with release assertions disabled.
        static void Check(bool condition,const char* message) { if (!condition) throw std::runtime_error(message); }
        /// Produces opaque 64-bit Vulkan handles used only by fake driver calls.
        template<typename T> static T Handle(uintptr_t value) { return reinterpret_cast<T>(value); }
        /// Seeds two frame arenas as if a preceding real Prepare had populated them.
        static void SeedArenas(AndroidSceneGpu& gpu) {
            gpu.VertexBuffer=Handle<VkBuffer>(2); gpu.VertexMemory=Handle<VkDeviceMemory>(3); gpu.VertexMapping=reinterpret_cast<void*>(4); gpu.VertexCapacity=uint64_t{8}*1024*1024;
            gpu.LightingBuffer=Handle<VkBuffer>(5); gpu.LightingMemory=Handle<VkDeviceMemory>(6); gpu.LightingMapping=reinterpret_cast<void*>(7); gpu.LightingCapacity=uint64_t{2}*1024*1024;
            gpu.FirstVertices.reserve(10000); gpu.FirstVertices.push_back(30);
        }
        /// Seeds a texture resource with the same source ownership relationship used by the real cache.
        static void SeedTexture(AndroidSceneGpu& gpu,const std::shared_ptr<SceneTexture>& source) {
            AndroidSceneGpu::TextureResource resource{}; resource.Source=source; resource.Revision=1;
            resource.Descriptor=Handle<VkDescriptorSet>(10); resource.View=Handle<VkImageView>(11); resource.Image=Handle<VkImage>(12); resource.Memory=Handle<VkDeviceMemory>(13);
            gpu.Textures[source.get()]=resource;
        }
        /// Device work ends before freeing only textures whose CPU source has no remaining owner.
        static void RetiresOnlyExpiredTextures() {
            FakeTrimApi::Reset(); AndroidVulkanDevice device; AndroidSceneGpu gpu(device);
            auto live=std::make_shared<SceneTexture>(); SeedTexture(gpu,live);
            { auto expired=std::make_shared<SceneTexture>(); SeedTexture(gpu,expired); }
            const auto result=gpu.TrimUnusedResources();
            Check(result.ExpiredTextureCount==1 && gpu.Textures.size()==1 && gpu.Textures.count(live.get())==1,"Trimming must preserve live authored texture sources.");
            Check(FakeTrimApi::Calls==std::vector<std::string>{"wait","descriptor","view","image","free"},"Expired textures must retire only after device completion and in descriptor/image/memory order.");
        }
        /// Rebuildable arenas clear mappings, handles and capacities so existing Prepare branches recreate their buffers.
        static void ReleasesArenas() {
            FakeTrimApi::Reset(); AndroidVulkanDevice device; AndroidSceneGpu gpu(device); SeedArenas(gpu);
            gpu.LightingDescriptor=Handle<VkDescriptorSet>(20); gpu.MeshPipeline=Handle<VkPipeline>(21);
            const auto result=gpu.TrimUnusedResources();
            Check(result.FrameBufferCount==2 && result.VertexCapacityBytes==8*1024*1024 && result.LightingCapacityBytes==2*1024*1024,"Released arena capacity must be reported without pretending to measure driver allocations.");
            Check(!gpu.VertexBuffer && !gpu.VertexMemory && !gpu.VertexMapping && !gpu.VertexCapacity && !gpu.LightingBuffer && !gpu.LightingMemory && !gpu.LightingMapping && !gpu.LightingCapacity,"Released arenas must clear every handle, mapping and resize gate.");
            Check(gpu.FirstVertices.empty() && gpu.FirstVertices.capacity()==0 && gpu.LightingDescriptor==Handle<VkDescriptorSet>(20) && gpu.MeshPipeline==Handle<VkPipeline>(21),"Frame offsets must release storage while reusable descriptors and pipelines remain intact.");
            Check(FakeTrimApi::Calls==std::vector<std::string>{"wait","unmap","buffer","free","unmap","buffer","free"},"Mapped buffers must unmap before destruction and memory release.");
        }
        /// Repeated notifications cannot free already retired resources or report their capacity again.
        static void RepeatedTrim() {
            FakeTrimApi::Reset(); AndroidVulkanDevice device; AndroidSceneGpu gpu(device); SeedArenas(gpu); gpu.TrimUnusedResources();
            FakeTrimApi::Calls.clear(); const auto result=gpu.TrimUnusedResources();
            Check(!result.FrameBufferCount && !result.VertexCapacityBytes && !result.LightingCapacityBytes && FakeTrimApi::Calls==std::vector<std::string>{"wait"},"Repeated trim must be idempotent after device synchronization.");
        }
        /// A device-loss failure during synchronization prevents freeing resources whose work may still be pending.
        static void DeviceWaitFailure() {
            FakeTrimApi::Reset(); AndroidVulkanDevice device; AndroidSceneGpu gpu(device); SeedArenas(gpu); FakeTrimApi::WaitFails=true; bool failed=false;
            try { gpu.TrimUnusedResources(); } catch (const std::runtime_error&) { failed=true; }
            Check(failed && FakeTrimApi::Calls==std::vector<std::string>{"wait"} && gpu.VertexBuffer && gpu.LightingBuffer,"Wait failure must propagate before any GPU resource is retired.");
        }
        /// Pressure before the first native window exists has no logical device or resource work to perform.
        static void ColdLifecycle() {
            FakeTrimApi::Reset(); FakeTrimApi::DeviceInitialized=false; AndroidVulkanDevice device; AndroidSceneGpu gpu(device);
            Check(gpu.TrimUnusedResources().FrameBufferCount==0 && FakeTrimApi::Calls.empty(),"Cold lifecycle trimming must not call a nonexistent logical device.");
        }
        /// JNI routes only to the matching still-live Activity without performing GPU work on the callback thread.
        static void ActivityRouting() {
            FakeTrimApi::Reset(); ANativeActivity activity{}; activity.clazz=reinterpret_cast<jobject>(100); ALooper looper;
            { AndroidMemoryTrimBridge bridge(&activity,&looper);
                Java_com_helengine_android_HelengineActivity_nativeRequestMemoryTrim(&FakeTrimApi::Environment,reinterpret_cast<jobject>(200),40);
                Check(bridge.TakePendingLevel()==0 && looper.Wakes==0,"Another Activity cannot queue work on this host.");
                Java_com_helengine_android_HelengineActivity_nativeRequestMemoryTrim(&FakeTrimApi::Environment,activity.clazz,20);
                Check(bridge.TakePendingLevel()==20 && looper.Wakes==1 && looper.References==1 && FakeTrimApi::Calls.empty(),"JNI must only queue and wake, leaving Vulkan cleanup to the owning native loop.");
            }
            Check(looper.References==0,"Bridge lifetime must retire its retained looper.");
        }
        /// Stronger requests coalesce until consumed; a new request after consumption wakes an inactive loop again.
        static void CoalescingAndWake() {
            ANativeActivity activity{}; activity.clazz=reinterpret_cast<jobject>(100); ALooper looper; AndroidMemoryTrimBridge bridge(&activity,&looper);
            bridge.Request(5); bridge.Request(10); bridge.Request(20); bridge.Request(15); bridge.Request(20); bridge.Request(40);
            Check(bridge.TakePendingLevel()==40 && bridge.TakePendingLevel()==0 && looper.Wakes==3,"Only stronger pending severities should add wake requests.");
            bridge.Request(11); Check(bridge.TakePendingLevel()==11 && looper.Wakes==4,"Intermediate levels must wake a suspended host after preceding cleanup was consumed.");
        }
        /// Emits concurrent UI-shaped notifications through the actual locked registration lookup.
        static void NotifyWorker(jobject activity,int level) { for (int index=0;index<100;++index) AndroidMemoryTrimBridge::NotifyActivity(&FakeTrimApi::Environment,activity,level); }
        /// Concurrent notifications retain the strongest pending level without duplicating renderer work.
        static void ConcurrentNotifications() {
            ANativeActivity activity{}; activity.clazz=reinterpret_cast<jobject>(100); ALooper looper; AndroidMemoryTrimBridge bridge(&activity,&looper);
            std::thread first(NotifyWorker,activity.clazz,20),second(NotifyWorker,activity.clazz,80); first.join(); second.join();
            Check(bridge.TakePendingLevel()==80 && looper.Wakes>=1 && looper.Wakes<=2,"Concurrent callbacks must coalesce without losing the strongest request.");
        }
        /// Callbacks after retirement and diagnostic-probe callbacks have no target and cannot access a released looper.
        static void LateCallbacksAndProbe() {
            ANativeActivity activity{}; activity.clazz=reinterpret_cast<jobject>(100); ALooper looper;
            { AndroidMemoryTrimBridge bridge(&activity,&looper); }
            Java_com_helengine_android_HelengineActivity_nativeRequestMemoryTrim(&FakeTrimApi::Environment,activity.clazz,80);
            Check(looper.Wakes==0 && looper.References==0,"Late and probe notifications must not resurrect a retired game host.");
            bool invalid=false; try { AndroidMemoryTrimBridge missing(&activity,nullptr); } catch (const std::invalid_argument&) { invalid=true; }
            Check(invalid,"A live registration requires a prepared native looper.");
        }
    };
}
/// Reports failed ownership or notification behavior through a nonzero process exit code.
int main() { try { helengine::android::AndroidSceneGpuMemoryTests::Run(); return 0; } catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; } }
