#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace helengine::android {
    /// Keeps internal target dimensions separate from physical display and input coordinates.
    class SceneResolutionMath {
    public:
        /// Rejects unsupported quality values before changing GPU resources.
        static void ValidateScale(double scale) {
            if (!std::isfinite(scale) || scale<0.25 || scale>1.0) {
                throw std::invalid_argument("Android 3D render scale must be finite and between 0.25 and 1.0.");
            }
        }
        /// Rounds one target dimension, retaining at least one pixel even for a small window.
        static std::uint32_t ScaleDimension(std::uint32_t pixels,double scale) {
            ValidateScale(scale);
            if (pixels==0) throw std::invalid_argument("A scene render surface must have nonzero dimensions.");
            return std::max<std::uint32_t>(1,static_cast<std::uint32_t>(std::floor(pixels*scale+0.5)));
        }
        /// Maps shared viewport edges through the actual rounded target extent so adjacent cameras meet at the same pixel.
        static std::uint32_t MapEdge(std::uint32_t pixel,std::uint32_t source,std::uint32_t target) {
            if (source==0 || target==0 || pixel>source) throw std::invalid_argument("A viewport edge must belong to its render surface.");
            return static_cast<std::uint32_t>(std::floor(static_cast<double>(pixel)*target/source+0.5));
        }
    };
}
