#pragma once
#include "IInputBackend.hpp"
#include "InputTouchTracker.hpp"
#include "AndroidGamepadBackend.hpp"
#include <android/input.h>
#include <android/native_activity.h>
#include <array>
#include <memory>

namespace helengine::android {
    /// Reports stable native contact IDs and mouse/controller state through reusable portable frame buffers.
    class AndroidInputBackend final : public ::IInputBackend {
    public:
        /// Creates alternating reusable controller snapshots so held buttons do not overwrite the previous engine frame.
        explicit AndroidInputBackend(ANativeActivity* activity);
        /// Releases the contact buffers after the engine finishes borrowing input frames.
        ~AndroidInputBackend();
        /// Returns the current pointer state and clears deltas after capture.
        ::InputFrameState CaptureFrame() override;
        /// Maps touch, mouse, controller axes and keyboard navigation into portable pointer and gamepad input.
        bool HandleEvent(AInputEvent* event);
        /// Clears held buttons when focus or the input queue is lost.
        void Reset();
        /// Forwards real activity start/stop commands to the controller device listener.
        void SetStarted(bool started) { Gamepads.SetStarted(started); }
    private:
        /// Borrows the environment attached by the renderer on this native host thread.
        static JNIEnv* GetEnvironment(ANativeActivity* activity);
        /// Translates pointer-index changes into stable contact IDs while preserving every lifetime transition.
        bool HandleTouchEvent(AInputEvent* event);
        /// Updates the legacy pointer from the original finger without promoting another held finger into a new click.
        void ProjectTouch(int32_t id, int32_t x, int32_t y, bool released);
        /// Shared contact tracker owns alternating frame buffers and ordered pending touch samples.
        ::InputTouchTracker Touches;
        /// Stable ID projected into the legacy pointer until that original contact ends.
        int32_t PrimaryTouchId = -1;
        /// Current pointer position, button state, and pending movement.
        ::InputPointerState Pointer;
        /// Retains a touch press until a frame captures it, including taps whose release arrives in the same event drain.
        bool PendingPrimaryPress = false;
        /// Owns standardized physical controller state independently from pointer and touch input.
        AndroidGamepadBackend Gamepads;
        /// Alternating snapshots borrowed by the engine for current/previous frame comparisons.
        std::array<std::unique_ptr<Array<::InputGamepadState>>,2> GamepadFrames;
        /// Next snapshot slot to publish.
        std::size_t NextFrame = 0;
    };
}
