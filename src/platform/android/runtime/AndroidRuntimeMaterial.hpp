#pragma once
#include "RuntimeMaterial.hpp"
#include "float4.hpp"

namespace helengine::android {
    /// Holds the Android standard material's cooked color and lighting policy.
    class AndroidRuntimeMaterial final : public ::RuntimeMaterial {
    public:
        /// Authored normalized RGBA tint consumed by the Vulkan mesh path.
        ::float4 Color;
        /// Whether the material receives the scene's directional lighting.
        bool Lit;
    };
}
