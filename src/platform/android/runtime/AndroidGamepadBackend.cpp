#include "AndroidGamepadBackend.hpp"
#include "InputGamepadButton.hpp"
#include <android/keycodes.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace helengine::android {
    AndroidGamepadBackend::AndroidGamepadBackend(JNIEnv* environment,jobject activity) : Environment(environment) {
        if (!Environment || !activity) throw std::runtime_error("Android gamepads require a live activity and attached host thread.");
        const auto result=Paddleboat_init(Environment,activity);
        if (result!=PADDLEBOAT_NO_ERROR) CheckJavaException();
        RequireSuccess(result);
        try {
            CheckJavaException();
            DeviceIds.fill(-1);
            Paddleboat_setControllerStatusCallback(ControllerStatusChanged,this);
        } catch (...) { Paddleboat_destroy(Environment); throw; }
    }
    AndroidGamepadBackend::~AndroidGamepadBackend() {
        Paddleboat_setControllerStatusCallback(nullptr,nullptr);
        Paddleboat_destroy(Environment);
    }
    void AndroidGamepadBackend::RequireSuccess(Paddleboat_ErrorCode result) {
        if (result!=PADDLEBOAT_NO_ERROR) throw std::runtime_error("Android Game Controller SDK failed: "+std::to_string(result));
    }
    void AndroidGamepadBackend::CheckJavaException() {
        if (Environment->ExceptionCheck()) {
            Environment->ExceptionClear();
            throw std::runtime_error("Android Game Controller SDK raised a Java exception.");
        }
    }
    void AndroidGamepadBackend::SetStarted(bool started) {
        if (Started==started) return;
        if (started) Paddleboat_onStart(Environment); else Paddleboat_onStop(Environment);
        CheckJavaException();
        Started=started;
        if (!started) Reset();
    }
    void AndroidGamepadBackend::ClearSlot(std::size_t index) {
        States[index]={}; DeviceIds[index]=-1; Pending[index]=0; BlockedButtons[index]=0; BlockedAxes[index]={};
    }
    void AndroidGamepadBackend::ControllerStatusChanged(int32_t index,Paddleboat_ControllerStatus status,void* owner) {
        if (index<0 || index>=PADDLEBOAT_MAX_CONTROLLERS || !owner) return;
        auto& backend=*static_cast<AndroidGamepadBackend*>(owner);
        if (status==PADDLEBOAT_CONTROLLER_JUST_DISCONNECTED) {
            // A queued callback may follow input already received from a replacement device.
            if (Paddleboat_getControllerStatus(index)!=PADDLEBOAT_CONTROLLER_ACTIVE) backend.ClearSlot(static_cast<std::size_t>(index));
            backend.Retired[index]=true;
        }
    }
    bool AndroidGamepadBackend::IsControllerEvent(const AInputEvent* event) const {
        const auto source=AInputEvent_getSource(event);
        if ((source&AINPUT_SOURCE_GAMEPAD)==AINPUT_SOURCE_GAMEPAD || (source&AINPUT_SOURCE_JOYSTICK)==AINPUT_SOURCE_JOYSTICK) return true;
        if (AInputEvent_getType(event)!=AINPUT_EVENT_TYPE_KEY) return false;
        for (int index=0;index<PADDLEBOAT_MAX_CONTROLLERS;++index) {
            if (Paddleboat_getControllerStatus(index)!=PADDLEBOAT_CONTROLLER_ACTIVE) continue;
            Paddleboat_Controller_Info info{};
            RequireSuccess(Paddleboat_getControllerInfo(index,&info));
            if (info.deviceId==AInputEvent_getDeviceId(event)) return true;
        }
        return false;
    }
    bool AndroidGamepadBackend::HandleEvent(const AInputEvent* event) {
        if (IsControllerEvent(event)) {
            const bool consumed=Paddleboat_processInputEvent(event)!=0;
            if (consumed) SampleControllers();
            // An undiscovered controller must never become keyboard player zero or a mouse.
            return true;
        }
        if (AInputEvent_getType(event)!=AINPUT_EVENT_TYPE_KEY) return false;
        const auto key=AKeyEvent_getKeyCode(event);
        if (KeyboardButton(key)<0) return false;
        const auto action=AKeyEvent_getAction(event);
        if (action!=AKEY_EVENT_ACTION_DOWN && action!=AKEY_EVENT_ACTION_UP) return false;
        if (action==AKEY_EVENT_ACTION_DOWN && AKeyEvent_getRepeatCount(event)>0 && !Keys[key]) return true;
        const auto previous=Keyboard.Buttons;
        Keys[key]=action==AKEY_EVENT_ACTION_DOWN && (AKeyEvent_getFlags(event)&AKEY_EVENT_FLAG_CANCELED)==0;
        UpdateKeyboard(); Keyboard.Connected=true;
        KeyboardPending|=Keyboard.Buttons&~previous;
        return true;
    }
    int AndroidGamepadBackend::KeyboardButton(int32_t key) {
        switch (key) {
            case AKEYCODE_DPAD_UP: case AKEYCODE_W: return static_cast<int>(::InputGamepadButton::DPadUp);
            case AKEYCODE_DPAD_DOWN: case AKEYCODE_S: return static_cast<int>(::InputGamepadButton::DPadDown);
            case AKEYCODE_DPAD_LEFT: case AKEYCODE_A: return static_cast<int>(::InputGamepadButton::DPadLeft);
            case AKEYCODE_DPAD_RIGHT: case AKEYCODE_D: return static_cast<int>(::InputGamepadButton::DPadRight);
            case AKEYCODE_DPAD_CENTER: case AKEYCODE_ENTER: case AKEYCODE_SPACE: case AKEYCODE_J: return static_cast<int>(::InputGamepadButton::South);
            case AKEYCODE_BACK: case AKEYCODE_ESCAPE: case AKEYCODE_K: return static_cast<int>(::InputGamepadButton::East);
            case AKEYCODE_U: return static_cast<int>(::InputGamepadButton::West);
            case AKEYCODE_I: return static_cast<int>(::InputGamepadButton::North);
            case AKEYCODE_Q: return static_cast<int>(::InputGamepadButton::LeftShoulder);
            case AKEYCODE_E: return static_cast<int>(::InputGamepadButton::RightShoulder);
            default: return -1;
        }
    }
    void AndroidGamepadBackend::UpdateKeyboard() {
        Keyboard.Buttons=0;
        for (std::size_t key=0;key<Keys.size();++key) {
            if (Keys[key]) Keyboard.Buttons|=uint64_t{1}<<KeyboardButton(static_cast<int32_t>(key));
        }
        Keyboard.LeftStickX=static_cast<int16_t>((Keyboard.IsButtonDown(::InputGamepadButton::DPadRight)?32767:0)-(Keyboard.IsButtonDown(::InputGamepadButton::DPadLeft)?32767:0));
        Keyboard.LeftStickY=static_cast<int16_t>((Keyboard.IsButtonDown(::InputGamepadButton::DPadDown)?32767:0)-(Keyboard.IsButtonDown(::InputGamepadButton::DPadUp)?32767:0));
    }
    int16_t AndroidGamepadBackend::ConvertAxis(float value,float flat,bool trigger) {
        if (!std::isfinite(value)) return 0;
        const double normalized=std::clamp(static_cast<double>(value),trigger?0.0:-1.0,1.0);
        const double hardwareFlat=std::isfinite(flat)?std::clamp(static_cast<double>(flat),0.0,1.0):0.0;
        return std::abs(normalized)<=hardwareFlat?0:static_cast<int16_t>(std::lround(normalized*32767.0));
    }
    ::InputGamepadState AndroidGamepadBackend::Translate(const Paddleboat_Controller_Data& data,const Paddleboat_Controller_Info& info) {
        ::InputGamepadState state{}; state.Connected=true;
        state.SetButtonDown(::InputGamepadButton::South,(data.buttonsDown&PADDLEBOAT_BUTTON_A)!=0);
        state.SetButtonDown(::InputGamepadButton::East,(data.buttonsDown&PADDLEBOAT_BUTTON_B)!=0);
        state.SetButtonDown(::InputGamepadButton::West,(data.buttonsDown&PADDLEBOAT_BUTTON_X)!=0);
        state.SetButtonDown(::InputGamepadButton::North,(data.buttonsDown&PADDLEBOAT_BUTTON_Y)!=0);
        state.SetButtonDown(::InputGamepadButton::LeftShoulder,(data.buttonsDown&PADDLEBOAT_BUTTON_L1)!=0);
        state.SetButtonDown(::InputGamepadButton::RightShoulder,(data.buttonsDown&PADDLEBOAT_BUTTON_R1)!=0);
        state.SetButtonDown(::InputGamepadButton::LeftStick,(data.buttonsDown&PADDLEBOAT_BUTTON_L3)!=0);
        state.SetButtonDown(::InputGamepadButton::RightStick,(data.buttonsDown&PADDLEBOAT_BUTTON_R3)!=0);
        state.SetButtonDown(::InputGamepadButton::LeftTrigger,(data.buttonsDown&PADDLEBOAT_BUTTON_L2)!=0);
        state.SetButtonDown(::InputGamepadButton::RightTrigger,(data.buttonsDown&PADDLEBOAT_BUTTON_R2)!=0);
        state.SetButtonDown(::InputGamepadButton::DPadUp,(data.buttonsDown&PADDLEBOAT_BUTTON_DPAD_UP)!=0);
        state.SetButtonDown(::InputGamepadButton::DPadDown,(data.buttonsDown&PADDLEBOAT_BUTTON_DPAD_DOWN)!=0);
        state.SetButtonDown(::InputGamepadButton::DPadLeft,(data.buttonsDown&PADDLEBOAT_BUTTON_DPAD_LEFT)!=0);
        state.SetButtonDown(::InputGamepadButton::DPadRight,(data.buttonsDown&PADDLEBOAT_BUTTON_DPAD_RIGHT)!=0);
        state.SetButtonDown(::InputGamepadButton::Start,(data.buttonsDown&PADDLEBOAT_BUTTON_START)!=0);
        state.SetButtonDown(::InputGamepadButton::Select,(data.buttonsDown&PADDLEBOAT_BUTTON_SELECT)!=0);
        state.SetButtonDown(::InputGamepadButton::Home,(data.buttonsDown&PADDLEBOAT_BUTTON_SYSTEM)!=0);
        state.LeftStickX=ConvertAxis(data.leftStick.stickX,info.leftStickPrecision.stickFlatX,false);
        state.LeftStickY=ConvertAxis(data.leftStick.stickY,info.leftStickPrecision.stickFlatY,false);
        state.RightStickX=ConvertAxis(data.rightStick.stickX,info.rightStickPrecision.stickFlatX,false);
        state.RightStickY=ConvertAxis(data.rightStick.stickY,info.rightStickPrecision.stickFlatY,false);
        state.LeftTrigger=ConvertAxis(data.triggerL2,0,true); state.RightTrigger=ConvertAxis(data.triggerR2,0,true);
        return state;
    }
    void AndroidGamepadBackend::SampleControllers() {
        for (int index=0;index<PADDLEBOAT_MAX_CONTROLLERS;++index) {
            if (Paddleboat_getControllerStatus(index)!=PADDLEBOAT_CONTROLLER_ACTIVE) { ClearSlot(index); continue; }
            Paddleboat_Controller_Data data{}; Paddleboat_Controller_Info info{};
            RequireSuccess(Paddleboat_getControllerData(index,&data)); RequireSuccess(Paddleboat_getControllerInfo(index,&info));
            if (DeviceIds[index]!=-1 && DeviceIds[index]!=info.deviceId) { ClearSlot(index); Retired[index]=true; }
            DeviceIds[index]=info.deviceId;
            auto state=Translate(data,info);
            BlockedButtons[index]&=state.Buttons; state.Buttons&=~BlockedButtons[index];
            std::array<int16_t*,6> axes={&state.LeftStickX,&state.LeftStickY,&state.RightStickX,&state.RightStickY,&state.LeftTrigger,&state.RightTrigger};
            for (std::size_t axis=0;axis<axes.size();++axis) {
                if (*axes[axis]==0) BlockedAxes[index][axis]=false;
                if (BlockedAxes[index][axis]) *axes[axis]=0;
            }
            Pending[index]|=state.Buttons&~States[index].Buttons; States[index]=state;
        }
    }
    int AndroidGamepadBackend::Capture(::InputGamepadState* destination) {
        if (!destination) throw std::invalid_argument("Android gamepad capture requires eight destination slots.");
        Paddleboat_update(Environment); CheckJavaException(); SampleControllers();
        int count=0;
        for (int index=0;index<PADDLEBOAT_MAX_CONTROLLERS;++index) {
            if (Retired[index]) { destination[index]={}; Retired[index]=false; }
            else { destination[index]=States[index]; destination[index].Buttons|=Pending[index]; Pending[index]=0; }
            if (index==0 && Keyboard.Connected) {
                destination[index].Connected=true; destination[index].Buttons|=Keyboard.Buttons|KeyboardPending;
                if (std::abs(Keyboard.LeftStickX)>std::abs(destination[index].LeftStickX)) destination[index].LeftStickX=Keyboard.LeftStickX;
                if (std::abs(Keyboard.LeftStickY)>std::abs(destination[index].LeftStickY)) destination[index].LeftStickY=Keyboard.LeftStickY;
            }
            if (destination[index].Connected) count=index+1;
        }
        KeyboardPending=0; return count;
    }
    void AndroidGamepadBackend::Reset() {
        for (int index=0;index<PADDLEBOAT_MAX_CONTROLLERS;++index) {
            if (Paddleboat_getControllerStatus(index)!=PADDLEBOAT_CONTROLLER_ACTIVE) { ClearSlot(index); continue; }
            Paddleboat_Controller_Data data{}; Paddleboat_Controller_Info info{};
            RequireSuccess(Paddleboat_getControllerData(index,&data)); RequireSuccess(Paddleboat_getControllerInfo(index,&info));
            const auto raw=Translate(data,info);
            BlockedButtons[index]=raw.Buttons;
            BlockedAxes[index]={raw.LeftStickX!=0,raw.LeftStickY!=0,raw.RightStickX!=0,raw.RightStickY!=0,raw.LeftTrigger!=0,raw.RightTrigger!=0};
            States[index]={}; Pending[index]=0;
        }
        Keys={}; Keyboard={}; KeyboardPending=0;
    }
}
