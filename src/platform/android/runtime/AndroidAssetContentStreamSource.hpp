#pragma once
#include "IContentStreamSource.hpp"
#include <android/asset_manager.h>

namespace helengine::android {
    /// Opens cooked runtime assets directly from the APK using the engine's existing content-source contract.
    class AndroidAssetContentStreamSource final : public ::IContentStreamSource {
    public:
        /// Borrows the activity's asset manager, which must outlive this source and every stream it opens.
        explicit AndroidAssetContentStreamSource(AAssetManager* manager);
        /// Returns a caller-owned, independent stream for one normalized cooked content path.
        ::Stream* OpenRead(HeCppString assetPath) override;
    private:
        /// Keeps content paths inside the cooked APK namespace while allowing ordinary relative dot segments.
        static std::string NormalizePath(const std::string& path);
        /// Shared SDK manager supports independent opens from different scene-loading workers.
        AAssetManager* Manager;
    };
}
