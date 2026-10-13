#pragma once
#include "RuntimeTexture.hpp"
#include "../rendering/AndroidSceneGpu.hpp"

namespace helengine::android {
    /// Retains decoded RGBA8 pixels until the Vulkan renderer has uploaded or released their last batch.
    class AndroidRuntimeTexture final : public ::RuntimeTexture {
    public:
        /// Pixel storage shared with queued draws and tracked weakly by the GPU image cache.
        std::shared_ptr<SceneTexture> Image;
        /// Releases CPU ownership while allowing an already queued frame to finish safely.
        void Dispose() override { Image.reset(); RuntimeTexture::Dispose(); }
    };
}
