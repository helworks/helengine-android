#include "FakeGamepadApi.hpp"

namespace {
    /// Returns an injected exception without simulating an unrelated Java VM.
    jboolean JNICALL ExceptionCheck(JNIEnv*) { return FakeGamepadApi::JavaException?JNI_TRUE:JNI_FALSE; }
    /// Clears an observed Java exception just as the production host bridge expects.
    void JNICALL ExceptionClear(JNIEnv*) { FakeGamepadApi::JavaException=false; }
    /// Creates the minimal JNI table required by real adapter methods.
    JNINativeInterface_ CreateJniTable() {
        JNINativeInterface_ table{}; table.ExceptionCheck=ExceptionCheck; table.ExceptionClear=ExceptionClear; return table;
    }
    /// JNI function table outlives every test backend.
    const JNINativeInterface_ JniTable=CreateJniTable();
}
JNIEnv FakeGamepadApi::Environment{&JniTable};
bool FakeGamepadApi::Initialized=false,FakeGamepadApi::JavaException=false;
int FakeGamepadApi::Starts=0,FakeGamepadApi::Stops=0,FakeGamepadApi::Updates=0,FakeGamepadApi::Destroys=0,FakeGamepadApi::Processed=0;
Paddleboat_ErrorCode FakeGamepadApi::InitResult=PADDLEBOAT_NO_ERROR,FakeGamepadApi::ReadResult=PADDLEBOAT_NO_ERROR;
std::array<Paddleboat_ControllerStatus,PADDLEBOAT_MAX_CONTROLLERS> FakeGamepadApi::Status{};
std::array<Paddleboat_Controller_Data,PADDLEBOAT_MAX_CONTROLLERS> FakeGamepadApi::Data{};
std::array<Paddleboat_Controller_Info,PADDLEBOAT_MAX_CONTROLLERS> FakeGamepadApi::Info{};
Paddleboat_ControllerStatusCallback FakeGamepadApi::Callback=nullptr;
void* FakeGamepadApi::Owner=nullptr;
std::array<bool,PADDLEBOAT_MAX_CONTROLLERS> FakeGamepadApi::Disconnects{};
void FakeGamepadApi::Reset() {
    Initialized=false; JavaException=false; Starts=Stops=Updates=Destroys=Processed=0;
    InitResult=ReadResult=PADDLEBOAT_NO_ERROR; Status={}; Data={}; Info={}; Callback=nullptr; Owner=nullptr; Disconnects={};
}
void FakeGamepadApi::Connect(int index,int deviceId) { Status[index]=PADDLEBOAT_CONTROLLER_ACTIVE; Data[index]={}; Info[index]={}; Info[index].deviceId=deviceId; }
void FakeGamepadApi::Disconnect(int index) { Status[index]=PADDLEBOAT_CONTROLLER_INACTIVE; Data[index]={}; Disconnects[index]=true; }
/// Implements SDK singleton initialization while preserving another owner's registration on failure.
Paddleboat_ErrorCode Paddleboat_init(JNIEnv*,jobject) {
    if (FakeGamepadApi::Initialized) return PADDLEBOAT_ERROR_ALREADY_INITIALIZED;
    if (FakeGamepadApi::InitResult!=PADDLEBOAT_NO_ERROR) return FakeGamepadApi::InitResult;
    FakeGamepadApi::Initialized=true; return PADDLEBOAT_NO_ERROR;
}
/// Reports fake singleton availability.
bool Paddleboat_isInitialized() { return FakeGamepadApi::Initialized; }
/// Observes callback retirement and tears down SDK state.
void Paddleboat_destroy(JNIEnv*) { ++FakeGamepadApi::Destroys; FakeGamepadApi::Initialized=false; }
/// Counts real activity starts.
void Paddleboat_onStart(JNIEnv*) { ++FakeGamepadApi::Starts; }
/// Counts real activity stops.
void Paddleboat_onStop(JNIEnv*) { ++FakeGamepadApi::Stops; }
/// Stores the real backend's callback and owner pointer.
void Paddleboat_setControllerStatusCallback(Paddleboat_ControllerStatusCallback callback,void* owner) { FakeGamepadApi::Callback=callback; FakeGamepadApi::Owner=owner; }
/// Delivers hotplug callbacks once per capture before the adapter samples final SDK state.
void Paddleboat_update(JNIEnv*) {
    ++FakeGamepadApi::Updates;
    for (int index=0;index<PADDLEBOAT_MAX_CONTROLLERS;++index) {
        if (FakeGamepadApi::Disconnects[index] && FakeGamepadApi::Callback) FakeGamepadApi::Callback(index,PADDLEBOAT_CONTROLLER_JUST_DISCONNECTED,FakeGamepadApi::Owner);
        FakeGamepadApi::Disconnects[index]=false;
    }
}
/// Reads connected status without compacting indices.
Paddleboat_ControllerStatus Paddleboat_getControllerStatus(int32_t index) { return FakeGamepadApi::Status[index]; }
/// Returns standardized controller data or an injected SDK failure.
Paddleboat_ErrorCode Paddleboat_getControllerData(int32_t index,Paddleboat_Controller_Data* data) {
    if (FakeGamepadApi::ReadResult!=PADDLEBOAT_NO_ERROR) return FakeGamepadApi::ReadResult;
    *data=FakeGamepadApi::Data[index]; return PADDLEBOAT_NO_ERROR;
}
/// Returns device identity and normalized hardware precision.
Paddleboat_ErrorCode Paddleboat_getControllerInfo(int32_t index,Paddleboat_Controller_Info* info) {
    if (FakeGamepadApi::ReadResult!=PADDLEBOAT_NO_ERROR) return FakeGamepadApi::ReadResult;
    *info=FakeGamepadApi::Info[index]; return PADDLEBOAT_NO_ERROR;
}
/// Applies a key or motion packet immediately, matching the SDK's synchronous native event processing.
int32_t Paddleboat_processInputEvent(const AInputEvent* event) {
    ++FakeGamepadApi::Processed;
    for (int index=0;index<PADDLEBOAT_MAX_CONTROLLERS;++index) {
        if (FakeGamepadApi::Status[index]!=PADDLEBOAT_CONTROLLER_ACTIVE || FakeGamepadApi::Info[index].deviceId!=event->DeviceId) continue;
        FakeGamepadApi::Data[index].buttonsDown=event->Buttons;
        if (event->Type==AINPUT_EVENT_TYPE_MOTION) { FakeGamepadApi::Data[index].leftStick.stickX=event->X; FakeGamepadApi::Data[index].leftStick.stickY=event->Y; }
        return 1;
    }
    return 0;
}
