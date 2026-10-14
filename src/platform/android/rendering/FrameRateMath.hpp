#pragma once
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace helengine::android {
    /// Converts the authored FPS contract into the minimum requested presentation period.
    class FrameRateMath {
    public:
        /// Accepts display-rate mode (zero) or an explicit whole-number target up to 240 FPS.
        static void Validate(double framesPerSecond) {
            if (!std::isfinite(framesPerSecond) || framesPerSecond < 0 || framesPerSecond > 240 || std::floor(framesPerSecond) != framesPerSecond) {
                throw std::invalid_argument("Android target frame rate must be a whole number between 0 and 240.");
            }
        }
        /// Zero uses a real display period, avoiding an unsupported frame-rate vote to Android's window manager.
        static std::uint64_t IntervalNanoseconds(double framesPerSecond, std::uint64_t displayPeriod) {
            Validate(framesPerSecond);
            if (framesPerSecond==0 && displayPeriod==0) throw std::invalid_argument("Display-rate mode requires a measured refresh period.");
            return framesPerSecond == 0 ? displayPeriod : static_cast<std::uint64_t>(std::llround(1000000000.0 / framesPerSecond));
        }
    };
}
