#include "AndroidInputBackend.hpp"
#include "InputPointerButton.hpp"
#include <cmath>

namespace helengine::android {
    ::InputFrameState AndroidInputBackend::CaptureFrame() {
        ::InputFrameState frame;
        frame.Pointer=Pointer;
        if (PendingPrimaryPress) frame.Pointer.SetButtonDown(::InputPointerButton::Primary,true);
        PendingPrimaryPress=false;
        Pointer.DeltaX=Pointer.DeltaY=Pointer.ScrollDelta=0;
        return frame;
    }
    bool AndroidInputBackend::HandleEvent(AInputEvent* event) {
        if (AInputEvent_getType(event)!=AINPUT_EVENT_TYPE_MOTION) return false;
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
    void AndroidInputBackend::Reset() { Pointer.Buttons=0; Pointer.DeltaX=Pointer.DeltaY=Pointer.ScrollDelta=0; PendingPrimaryPress=false; }
}
