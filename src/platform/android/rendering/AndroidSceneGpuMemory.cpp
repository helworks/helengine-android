#include "AndroidSceneGpu.hpp"
#include "../diagnostics/AndroidTraceSection.hpp"

namespace helengine::android {
    void AndroidSceneGpu::DestroyTexture(TextureResource& texture) {
        const auto device=Device.GetDevice();
        if (texture.Descriptor) vkFreeDescriptorSets(device,DescriptorPool,1,&texture.Descriptor);
        if (texture.View) vkDestroyImageView(device,texture.View,nullptr);
        if (texture.Image) vkDestroyImage(device,texture.Image,nullptr);
        if (texture.Memory) vkFreeMemory(device,texture.Memory,nullptr);
        texture={};
    }
    void AndroidSceneGpu::ReleaseFrameBuffers() {
        const auto device=Device.GetDevice();
        if (VertexMapping) vkUnmapMemory(device,VertexMemory);
        if (VertexBuffer) vkDestroyBuffer(device,VertexBuffer,nullptr);
        if (VertexMemory) vkFreeMemory(device,VertexMemory,nullptr);
        VertexMapping=nullptr; VertexBuffer=VK_NULL_HANDLE; VertexMemory=VK_NULL_HANDLE; VertexCapacity=0;
        if (LightingMapping) vkUnmapMemory(device,LightingMemory);
        if (LightingBuffer) vkDestroyBuffer(device,LightingBuffer,nullptr);
        if (LightingMemory) vkFreeMemory(device,LightingMemory,nullptr);
        LightingMapping=nullptr; LightingBuffer=VK_NULL_HANDLE; LightingMemory=VK_NULL_HANDLE; LightingCapacity=0;
    }
    uint64_t AndroidSceneGpu::PruneExpiredTextures() {
        uint64_t count=0;
        for (auto iterator=Textures.begin();iterator!=Textures.end();) {
            if (iterator->second.Source.expired()) { DestroyTexture(iterator->second); iterator=Textures.erase(iterator); ++count; }
            else ++iterator;
        }
        return count;
    }
    AndroidMemoryTrimResult AndroidSceneGpu::TrimUnusedResources() {
        const AndroidTraceSection trace("HelEngine.Memory.TrimGpu");
        AndroidMemoryTrimResult result{};
        if (!Device.IsInitialized()) return result;
        Device.WaitIdle();
        result.ExpiredTextureCount=PruneExpiredTextures();
        result.FrameBufferCount=(VertexBuffer!=VK_NULL_HANDLE?1:0)+(LightingBuffer!=VK_NULL_HANDLE?1:0);
        result.VertexCapacityBytes=VertexCapacity; result.LightingCapacityBytes=LightingCapacity;
        ReleaseFrameBuffers(); std::vector<uint32_t>().swap(FirstVertices);
        return result;
    }
}
