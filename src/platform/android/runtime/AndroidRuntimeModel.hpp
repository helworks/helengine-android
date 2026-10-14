#pragma once
#include "RuntimeModel.hpp"
#include "float3.hpp"
#include "float2.hpp"
#include <vector>

namespace helengine::android {
    /// Owns copied authored mesh geometry independently of the cooked model's deserialization lifetime.
    class AndroidRuntimeModel final : public ::RuntimeModel {
    public:
        /// Model-space positions uploaded through the scene renderer.
        std::vector<::float3> Positions;
        /// Model-space normals transformed for directional lighting.
        std::vector<::float3> Normals;
        /// Authored diffuse texture coordinates, retained independently of the source asset.
        std::vector<::float2> TexCoords;
        /// Triangle-list indices referenced by the inherited submesh ranges.
        std::vector<std::uint32_t> Indices;
    };
}
