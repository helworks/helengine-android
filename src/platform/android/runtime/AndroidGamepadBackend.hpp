#pragma once
#include "InputGamepadState.hpp"
#include <paddleboat/paddleboat.h>
#include <array>

namespace helengine::android {
    /// Publishes independent Paddleboat controller slots and overlays keyboard navigation on slot zero.
    class AndroidGamepadBackend final {
    public:
        /// Initializes the SDK on the attached native host thread using its live Android activity.
        AndroidGamepadBackend(JNIEnv* environment,jobject activity);
        /// Unregisters callbacks before releasing SDK resources on the same host thread.
        ~AndroidGamepadBackend();
        /// Prevents multiple owners of Paddleboat's process-wide registration.
        AndroidGamepadBackend(const AndroidGamepadBackend&)=delete;
        /// Prevents copying the SDK lifetime and callback target.
        AndroidGamepadBackend& operator=(const AndroidGamepadBackend&)=delete;
        /// Forwards activity start/stop transitions without coupling them to surface changes.
        void SetStarted(bool started);
        /// Consumes managed controller events or supported keyboard navigation keys, leaving pointers untouched.
        bool HandleEvent(const AInputEvent* event);
        /// Updates hotplug once per frame, fills all eight reusable slots and returns their occupied extent.
        int Capture(::InputGamepadState* destination);
        /// Clears pending actions and suppresses held physical controls until they return to neutral.
        void Reset();
    private:
        /// Records retirement so a replacement controller cannot inherit the previous device's held actions.
        static void ControllerStatusChanged(int32_t index,Paddleboat_ControllerStatus status,void* owner);
        /// Throws on native SDK errors rather than publishing a fabricated device state.
        static void RequireSuccess(Paddleboat_ErrorCode result);
        /// Reports and clears pending JNI exceptions before returning to the host event loop.
        void CheckJavaException();
        /// Recognizes controller source bits and keyboard-shaped events emitted by a managed device.
        bool IsControllerEvent(const AInputEvent* event) const;
        /// Samples SDK state after each consumed event so down/up pairs survive the next capture.
        void SampleControllers();
        /// Translates standardized SDK buttons and normalized axes into the existing engine ABI.
        static ::InputGamepadState Translate(const Paddleboat_Controller_Data& data,const Paddleboat_Controller_Info& info);
        /// Converts finite normalized axes to signed engine precision, honoring only the device's hardware flat region.
        static int16_t ConvertAxis(float value,float flat,bool trigger);
        /// Maps existing keyboard aliases to portable gamepad buttons, returning -1 for unsupported keys.
        static int KeyboardButton(int32_t key);
        /// Rebuilds keyboard buttons from individual keys so releasing one alias preserves another held alias.
        void UpdateKeyboard();
        /// Clears one retired slot without touching independent controllers or keyboard input.
        void ClearSlot(std::size_t index);
        /// JNI environment borrowed while the renderer keeps this host thread attached.
        JNIEnv* Environment;
        /// SDK listener registration follows real activity start/stop commands.
        bool Started=false;
        /// Current physical state indexed by the stable SDK slot, including disconnected holes.
        std::array<::InputGamepadState,PADDLEBOAT_MAX_CONTROLLERS> States{};
        /// Detects replacement devices even when their first input precedes a queued hotplug callback.
        std::array<int32_t,PADDLEBOAT_MAX_CONTROLLERS> DeviceIds{};
        /// Physical button presses retained until an engine frame observes them.
        std::array<uint64_t,PADDLEBOAT_MAX_CONTROLLERS> Pending{};
        /// Held buttons suppressed across focus loss until a release is observed.
        std::array<uint64_t,PADDLEBOAT_MAX_CONTROLLERS> BlockedButtons{};
        /// Six independent axis gates prevent cached movement from reappearing after a reset.
        std::array<std::array<bool,6>,PADDLEBOAT_MAX_CONTROLLERS> BlockedAxes{};
        /// Forces a disconnected frame between different devices reusing the same SDK slot.
        std::array<bool,PADDLEBOAT_MAX_CONTROLLERS> Retired{};
        /// Native keyboard aliases are tracked separately from physical controller state.
        std::array<bool,128> Keys{};
        /// Keyboard navigation is projected onto player zero without overwriting physical axes.
        ::InputGamepadState Keyboard{};
        /// Keyboard taps retained when both key transitions precede a frame capture.
        uint64_t KeyboardPending=0;
    };
}
