#pragma once
#include "SceneFrameOperation.hpp"
#include <cstddef>

namespace helengine::android {
    /// References an original batch without copying geometry or changing engine render-queue order.
    struct SceneFrameCommand {
        /// Selects the pass transition or draw operation.
        SceneFrameOperation Operation;
        /// Identifies the geometry batch or camera marker supplying clear policy and viewport.
        std::size_t BatchIndex;
    };
}
