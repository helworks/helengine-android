#pragma once
#include "Asset.hpp"
#include "AssetSerializer.hpp"
#include "IContentStreamSource.hpp"
#include "system/io/stream.hpp"
#include <memory>

namespace helengine::android {
    /// Deserializes a packaged engine asset using the host's configured stream source.
    inline std::unique_ptr<::Asset> ReadAsset(const std::string& path, ::IContentStreamSource* source) {
        if (!source || path.empty()) throw std::invalid_argument("Android asset loading requires a content source and path.");
        std::unique_ptr<::Stream> stream(source->OpenRead(path));
        return std::unique_ptr<::Asset>(::AssetSerializer::Deserialize(stream.get()));
    }
}
