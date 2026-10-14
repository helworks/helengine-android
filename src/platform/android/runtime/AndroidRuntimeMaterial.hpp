#pragma once
#include "RuntimeMaterial.hpp"
#include "float4.hpp"
#include "../rendering/AndroidSceneGpu.hpp"

namespace helengine::android {
    /// Holds the Android standard material's cooked color and lighting policy.
    class AndroidRuntimeMaterial final : public ::RuntimeMaterial {
    public:
        /// Authored normalized RGBA tint consumed by the Vulkan mesh path.
        ::float4 Color;
        /// Whether the material receives the scene's directional lighting.
        bool Lit;
        /// Resident diffuse pixels shared with the Vulkan texture cache and released with the material.
        std::shared_ptr<SceneTexture> Texture;
        /// Scalar surface coefficients used by the Cook-Torrance fragment shader.
        float Metallic=0, Roughness=0.5f, Specular=0.5f;
        /// Authored participation in directional shadow casting and receiving.
        bool CastShadows=true, ReceiveShadows=true;
        /// Opaque, alpha-tested or alpha-blended rendering policy.
        std::uint8_t AlphaMode=0;
        /// Optional grayscale modulation of the authored roughness scalar.
        std::shared_ptr<SceneTexture> RoughnessTexture;
    };
}
