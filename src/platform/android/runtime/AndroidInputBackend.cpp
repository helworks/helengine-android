#include "AndroidInputBackend.hpp"
#include "InputPointerButton.hpp"
#include <cmath>
#include <stdexcept>

namespace helengine::android {
    JNIEnv* AndroidInputBackend::GetEnvironment(ANativeActivity* activity) {
        JNIEnv* environment=nullptr;
        if (!activity || !activity->vm || !activity->clazz || activity->vm->GetEnv(reinterpret_cast<void**>(&environment),JNI_VERSION_1_6)!=JNI_OK) {
            throw std::runtime_error("Android input requires a live activity and attached host thread.");
        }
        return environment;
    }
    AndroidInputBackend::AndroidInputBackend(ANativeActivity* activity) : Gamepads(GetEnvironment(activity),activity?activity->clazz:nullptr) {
        for (auto& frame : GamepadFrames) frame=std::make_unique<Array<::InputGamepadState>>(PADDLEBOAT_MAX_CONTROLLERS);
    }
    AndroidInputBackend::~AndroidInputBackend() { Touches.Dispose(); }
    ::InputFrameState AndroidInputBackend::CaptureFrame() {
        ::InputFrameState frame=Touches.CaptureFrame();
        frame.Pointer=Pointer;
        frame.Gamepads=GamepadFrames[NextFrame].get();
        frame.GamepadCount=Gamepads.Capture(&(*GamepadFrames[NextFrame])[0]);
        NextFrame^=1;
        if (PendingPrimaryPress) frame.Pointer.SetButtonDown(::InputPointerButton::Primary,true);
        PendingPrimaryPress=false;
        Pointer.DeltaX=Pointer.DeltaY=Pointer.ScrollDelta=0;
        return frame;
    }
    bool AndroidInputBackend::HandleEvent(AInputEvent* event) {
        if (Gamepads.HandleEvent(event)) return true;
        if (AInputEvent_getType(event)!=AINPUT_EVENT_TYPE_MOTION) return false;
        if ((AInputEvent_getSource(event)&AINPUT_SOURCE_TOUCHSCREEN)==AINPUT_SOURCE_TOUCHSCREEN) {
            return HandleTouchEvent(event);
        }
        const auto action=AMotionEvent_getAction(event)&AMOTION_EVENT_ACTION_MASK;
        if (AMotionEvent_getPointerCount(event)==0) return false;
        int x=std::lround(AMotionEvent_getX(event,0)),y=std::lround(AMotionEvent_getY(event,0));
        if (Pointer.Connected && action!=AMOTION_EVENT_ACTION_DOWN) { Pointer.DeltaX+=x-Pointer.X; Pointer.DeltaY+=y-Pointer.Y; }
        Pointer.Connected=true; Pointer.IsTouch=false; Pointer.X=x; Pointer.Y=y;
        int buttons=AMotionEvent_getButtonState(event);
        Pointer.SetButtonDown(::InputPointerButton::Primary,(buttons&AMOTION_EVENT_BUTTON_PRIMARY)!=0);
        Pointer.SetButtonDown(::InputPointerButton::Secondary,(buttons&AMOTION_EVENT_BUTTON_SECONDARY)!=0);
        Pointer.ScrollDelta+=std::lround(AMotionEvent_getAxisValue(event,AMOTION_EVENT_AXIS_VSCROLL,0)*120);
        return true;
    }
    bool AndroidInputBackend::HandleTouchEvent(AInputEvent* event) {
        const int32_t nativeAction=AMotionEvent_getAction(event);
        const int32_t action=nativeAction&AMOTION_EVENT_ACTION_MASK;
        if (action==AMOTION_EVENT_ACTION_CANCEL) {
            Touches.CancelAll(); PrimaryTouchId=-1; PendingPrimaryPress=false;
            Pointer.IsTouch=true; Pointer.SetButtonDown(::InputPointerButton::Primary,false);
            return true;
        }
        const std::size_t count=AMotionEvent_getPointerCount(event);
        if (count==0) return false;
        const bool begin=action==AMOTION_EVENT_ACTION_DOWN || action==AMOTION_EVENT_ACTION_POINTER_DOWN;
        const bool end=action==AMOTION_EVENT_ACTION_UP || action==AMOTION_EVENT_ACTION_POINTER_UP;
        if (!begin && !end && action!=AMOTION_EVENT_ACTION_MOVE) return false;
        const std::size_t actionIndex=(nativeAction&AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)>>AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        if ((begin || end) && actionIndex>=count) return false;
        if (action==AMOTION_EVENT_ACTION_DOWN) {
            if (Touches.get_ActiveTouchCount()>0) Touches.CancelAll();
            PrimaryTouchId=-1;
            Pointer.SetButtonDown(::InputPointerButton::Primary,false); PendingPrimaryPress=false;
        }
        for (std::size_t index=0;index<count;++index) {
            const int32_t id=AMotionEvent_getPointerId(event,index);
            const int32_t x=std::lround(AMotionEvent_getX(event,index));
            const int32_t y=std::lround(AMotionEvent_getY(event,index));
            const float pressure=AMotionEvent_getPressure(event,index);
            if (begin && index==actionIndex) {
                const bool first=Touches.get_ActiveTouchCount()==0;
                Touches.BeginTouch(id,x,y,pressure);
                if (first) {
                    PrimaryTouchId=id; Pointer.Connected=true; Pointer.IsTouch=true;
                    Pointer.X=x; Pointer.Y=y; Pointer.DeltaX=Pointer.DeltaY=0;
                    Pointer.SetButtonDown(::InputPointerButton::Primary,true); PendingPrimaryPress=true;
                }
            } else if (end && index==actionIndex) {
                Touches.EndTouch(id,x,y,pressure);
                ProjectTouch(id,x,y,true);
            } else {
                Touches.MoveTouch(id,x,y,pressure);
                ProjectTouch(id,x,y,false);
            }
        }
        return true;
    }
    void AndroidInputBackend::ProjectTouch(int32_t id,int32_t x,int32_t y,bool released) {
        if (id!=PrimaryTouchId) return;
        Pointer.Connected=true; Pointer.IsTouch=true;
        Pointer.DeltaX+=x-Pointer.X; Pointer.DeltaY+=y-Pointer.Y; Pointer.X=x; Pointer.Y=y;
        if (released) { Pointer.SetButtonDown(::InputPointerButton::Primary,false); PrimaryTouchId=-1; }
    }
    void AndroidInputBackend::Reset() {
        Touches.CancelAll(); PrimaryTouchId=-1;
        Pointer.Buttons=0; Pointer.DeltaX=Pointer.DeltaY=Pointer.ScrollDelta=0; PendingPrimaryPress=false;
        Gamepads.Reset();
    }
}
