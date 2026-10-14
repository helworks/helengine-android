#pragma once

namespace helengine::android {
    /// Matches the std140 fragment-shader block for one camera/material draw, including all portable light kinds.
    struct alignas(16) SceneLighting {
        /// Camera world position used to compute specular view directions.
        float Camera[4]{};
        /// Accumulated authored ambient irradiance.
        float Ambient[4]{};
        /// Metallic, roughness, dielectric specular and lit-mode coefficients.
        float Surface[4]{};
        /// Alpha policy, roughness-map presence, light count and attachment sRGB encoding flag.
        float Options[4]{};
        /// Directional vectors or point/spot positions; w identifies directional, point or spot lighting.
        float LightPositions[8][4]{};
        /// Authored RGB radiance multiplied by light intensity.
        float LightColors[8][4]{};
        /// Spot-light cone directions in world space.
        float LightDirections[8][4]{};
        /// Local range and cosine of inner/outer spot angles.
        float LightParameters[8][4]{};
        /// Column-major orthographic light projection fitted to this frame's caster bounds.
        float ShadowMatrix[16]{};
        /// Shadow enabled, comparison bias, receiver policy and authored shadow cutoff distance.
        float ShadowParameters[4]{};
    };
}
