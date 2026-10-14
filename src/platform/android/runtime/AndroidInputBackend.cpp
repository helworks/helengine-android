#include "AndroidInputBackend.hpp"
#include "InputPointerButton.hpp"
#include "InputGamepadButton.hpp"
#include <android/keycodes.h>
#include <algorithm>
#include <cmath>

namespace helengine::android {
    AndroidInputBackend::AndroidInputBackend() {
        for (auto& frame : GamepadFrames) frame=std::make_unique<Array<::InputGamepadState>>(1);
        Gamepad.Connected=true;
    }
    ::InputFrameState AndroidInputBackend::CaptureFrame() {
        ::InputFrameState frame;
        frame.Pointer=Pointer;
        auto& snapshot=(*GamepadFrames[NextFrame])[0];
        snapshot=Gamepad;
        snapshot.Buttons|=PendingButtons;
        frame.Gamepads=GamepadFrames[NextFrame].get(); frame.GamepadCount=1;
        NextFrame^=1; PendingButtons=0;
        if (PendingPrimaryPress) frame.Pointer.SetButtonDown(::InputPointerButton::Primary,true);
        PendingPrimaryPress=false;
        Pointer.DeltaX=Pointer.DeltaY=Pointer.ScrollDelta=0;
        return frame;
    }
    bool AndroidInputBackend::HandleEvent(AInputEvent* event) {
        if (AInputEvent_getType(event)==AINPUT_EVENT_TYPE_KEY) {
            ::InputGamepadButton button;
            switch (AKeyEvent_getKeyCode(event)) {
                case AKEYCODE_DPAD_UP: case AKEYCODE_W: button=::InputGamepadButton::DPadUp; break;
                case AKEYCODE_DPAD_DOWN: case AKEYCODE_S: button=::InputGamepadButton::DPadDown; break;
                case AKEYCODE_DPAD_LEFT: case AKEYCODE_A: button=::InputGamepadButton::DPadLeft; break;
                case AKEYCODE_DPAD_RIGHT: case AKEYCODE_D: button=::InputGamepadButton::DPadRight; break;
                case AKEYCODE_DPAD_CENTER: case AKEYCODE_ENTER: case AKEYCODE_SPACE: case AKEYCODE_J: case AKEYCODE_BUTTON_A: button=::InputGamepadButton::South; break;
                case AKEYCODE_BACK: case AKEYCODE_ESCAPE: case AKEYCODE_K: case AKEYCODE_BUTTON_B: button=::InputGamepadButton::East; break;
                case AKEYCODE_BUTTON_X: case AKEYCODE_U: button=::InputGamepadButton::West; break;
                case AKEYCODE_BUTTON_Y: case AKEYCODE_I: button=::InputGamepadButton::North; break;
                case AKEYCODE_BUTTON_L1: case AKEYCODE_Q: button=::InputGamepadButton::LeftShoulder; break;
                case AKEYCODE_BUTTON_R1: case AKEYCODE_E: button=::InputGamepadButton::RightShoulder; break;
                case AKEYCODE_BUTTON_START: button=::InputGamepadButton::Start; break;
                case AKEYCODE_BUTTON_SELECT: button=::InputGamepadButton::Select; break;
                default: return false;
            }
            const bool down=AKeyEvent_getAction(event)==AKEY_EVENT_ACTION_DOWN;
            const auto mask=std::uint64_t{1}<<static_cast<int>(button);
            if (down && !Gamepad.IsButtonDown(button)) PendingButtons|=mask;
            Gamepad.SetButtonDown(button,down);
            Gamepad.LeftStickX=(Gamepad.IsButtonDown(::InputGamepadButton::DPadRight)?32767:0)-(Gamepad.IsButtonDown(::InputGamepadButton::DPadLeft)?32767:0);
            Gamepad.LeftStickY=(Gamepad.IsButtonDown(::InputGamepadButton::DPadDown)?32767:0)-(Gamepad.IsButtonDown(::InputGamepadButton::DPadUp)?32767:0);
            return true;
        }
        if (AInputEvent_getType(event)!=AINPUT_EVENT_TYPE_MOTION) return false;
        if ((AInputEvent_getSource(event)&AINPUT_SOURCE_JOYSTICK)==AINPUT_SOURCE_JOYSTICK) {
            Gamepad.LeftStickX=std::lround(std::clamp(AMotionEvent_getAxisValue(event,AMOTION_EVENT_AXIS_X,0),-1.0f,1.0f)*32767);
            Gamepad.LeftStickY=std::lround(std::clamp(AMotionEvent_getAxisValue(event,AMOTION_EVENT_AXIS_Y,0),-1.0f,1.0f)*32767);
            Gamepad.RightStickX=std::lround(std::clamp(AMotionEvent_getAxisValue(event,AMOTION_EVENT_AXIS_Z,0),-1.0f,1.0f)*32767);
            Gamepad.RightStickY=std::lround(std::clamp(AMotionEvent_getAxisValue(event,AMOTION_EVENT_AXIS_RZ,0),-1.0f,1.0f)*32767);
            Gamepad.LeftTrigger=std::lround(std::clamp(AMotionEvent_getAxisValue(event,AMOTION_EVENT_AXIS_LTRIGGER,0),0.0f,1.0f)*32767);
            Gamepad.RightTrigger=std::lround(std::clamp(AMotionEvent_getAxisValue(event,AMOTION_EVENT_AXIS_RTRIGGER,0),0.0f,1.0f)*32767);
            Gamepad.SetButtonDown(::InputGamepadButton::DPadLeft,AMotionEvent_getAxisValue(event,AMOTION_EVENT_AXIS_HAT_X,0)<-0.5f);
            Gamepad.SetButtonDown(::InputGamepadButton::DPadRight,AMotionEvent_getAxisValue(event,AMOTION_EVENT_AXIS_HAT_X,0)>0.5f);
            Gamepad.SetButtonDown(::InputGamepadButton::DPadUp,AMotionEvent_getAxisValue(event,AMOTION_EVENT_AXIS_HAT_Y,0)<-0.5f);
            Gamepad.SetButtonDown(::InputGamepadButton::DPadDown,AMotionEvent_getAxisValue(event,AMOTION_EVENT_AXIS_HAT_Y,0)>0.5f);
            return true;
        }
        const auto action=AMotionEvent_getAction(event)&AMOTION_EVENT_ACTION_MASK;
        if (AMotionEvent_getPointerCount(event)==0) return false;
        int x=std::lround(AMotionEvent_getX(event,0)),y=std::lround(AMotionEvent_getY(event,0));
        if (Pointer.Connected && action!=AMOTION_EVENT_ACTION_DOWN) { Pointer.DeltaX+=x-Pointer.X; Pointer.DeltaY+=y-Pointer.Y; }
        Pointer.Connected=true; Pointer.X=x; Pointer.Y=y;
        const bool touch=(AInputEvent_getSource(event)&AINPUT_SOURCE_TOUCHSCREEN)==AINPUT_SOURCE_TOUCHSCREEN;
        if (touch) {
            if (action==AMOTION_EVENT_ACTION_DOWN) { Pointer.SetButtonDown(::InputPointerButton::Primary,true); PendingPrimaryPress=true; }
            else if (action==AMOTION_EVENT_ACTION_UP) Pointer.SetButtonDown(::InputPointerButton::Primary,false);
            else if (action==AMOTION_EVENT_ACTION_CANCEL) Reset();
        } else {
            int buttons=AMotionEvent_getButtonState(event);
            Pointer.SetButtonDown(::InputPointerButton::Primary,(buttons&AMOTION_EVENT_BUTTON_PRIMARY)!=0);
            Pointer.SetButtonDown(::InputPointerButton::Secondary,(buttons&AMOTION_EVENT_BUTTON_SECONDARY)!=0);
            Pointer.ScrollDelta+=std::lround(AMotionEvent_getAxisValue(event,AMOTION_EVENT_AXIS_VSCROLL,0)*120);
        }
        return true;
    }
    void AndroidInputBackend::Reset() {
        Pointer.Buttons=0; Pointer.DeltaX=Pointer.DeltaY=Pointer.ScrollDelta=0; PendingPrimaryPress=false;
        Gamepad={}; Gamepad.Connected=true; PendingButtons=0;
    }
}
