#pragma once
#include "IInputBackend.hpp"
#include <android/input.h>

namespace helengine::android {
    /// Maps native touch and mouse events to the engine's portable pointer snapshot without per-frame allocation.
    class AndroidInputBackend final : public ::IInputBackend {
    public:
        /// Returns the current pointer state and clears deltas after capture.
        ::InputFrameState CaptureFrame() override;
        /// Consumes native motion events while leaving system navigation keys to Android.
        bool HandleEvent(AInputEvent* event);
        /// Clears held buttons when focus or the input queue is lost.
        void Reset();
    private:
        /// Current pointer position, button state, and pending movement.
        ::InputPointerState Pointer;
        /// Retains a touch press until a frame captures it, including taps whose release arrives in the same event drain.
        bool PendingPrimaryPress = false;
    };
}
