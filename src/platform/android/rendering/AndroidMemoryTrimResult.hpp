#pragma once
#include <cstdint>

namespace helengine::android {
    /// Reports released renderer resources without claiming their requested capacity is total driver memory usage.
    struct AndroidMemoryTrimResult {
        /// Number of formerly owned frame buffers released after device work completes.
        uint64_t FrameBufferCount=0;
        /// Number of cached GPU textures whose CPU source has no remaining owner.
        uint64_t ExpiredTextureCount=0;
        /// Requested capacity of the released vertex arena, excluding Vulkan allocation alignment overhead.
        uint64_t VertexCapacityBytes=0;
        /// Requested capacity of the released lighting arena, excluding Vulkan allocation alignment overhead.
        uint64_t LightingCapacityBytes=0;
    };
}
