#pragma once
#include "IInputBackend.hpp"
#include <android/input.h>
#include <array>
#include <memory>

namespace helengine::android {
    /// Maps native touch and mouse events to the engine's portable pointer snapshot without per-frame allocation.
    class AndroidInputBackend final : public ::IInputBackend {
    public:
        /// Creates alternating reusable controller snapshots so held buttons do not overwrite the previous engine frame.
        AndroidInputBackend();
        /// Returns the current pointer state and clears deltas after capture.
        ::InputFrameState CaptureFrame() override;
        /// Maps touch, mouse, controller axes and keyboard navigation into portable pointer and gamepad input.
        bool HandleEvent(AInputEvent* event);
        /// Clears held buttons when focus or the input queue is lost.
        void Reset();
    private:
        /// Current pointer position, button state, and pending movement.
        ::InputPointerState Pointer;
        /// Retains a touch press until a frame captures it, including taps whose release arrives in the same event drain.
        bool PendingPrimaryPress = false;
        /// Held portable gamepad state produced by native keys and joystick events.
        ::InputGamepadState Gamepad;
        /// Presses retained until capture, including down/up pairs drained before the next engine update.
        std::uint64_t PendingButtons = 0;
        /// Alternating snapshots borrowed by the engine for current/previous frame comparisons.
        std::array<std::unique_ptr<Array<::InputGamepadState>>,2> GamepadFrames;
        /// Next snapshot slot to publish.
        std::size_t NextFrame = 0;
    };
}
