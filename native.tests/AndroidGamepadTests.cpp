#include "../src/platform/android/runtime/AndroidGamepadBackend.hpp"
#include "gamepad.fakes/FakeGamepadApi.hpp"
#include <android/keycodes.h>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

/// Exercises the actual adapter against deterministic SDK and JNI boundaries without an Android device.
class AndroidGamepadTests {
public:
    /// Runs independent scenarios and fails immediately on a broken observable input contract.
    static void Run() {
        Lifecycle(); IndependentSlots(); ButtonsAndAxes(); AnalogSurvivesKey(); QuickTap(); KeyboardAliases();
        KeyboardAndController(); FocusReset(); KeyboardRepeatReset(); DisconnectAndReplacement(); PointerRouting(); Failures();
        std::cout<<"12 Android gamepad scenarios passed.\n";
    }
private:
    /// Real backend type compiled from production sources.
    using Backend=helengine::android::AndroidGamepadBackend;
    /// Eight caller-owned snapshots satisfy the capture contract.
    using Frame=std::array<InputGamepadState,PADDLEBOAT_MAX_CONTROLLERS>;
    /// Throws a descriptive assertion without relying on disabled release-build asserts.
    static void Check(bool condition,const char* message) { if (!condition) throw std::runtime_error(message); }
    /// Creates a backend with a real JNI interface table and an opaque nonnull activity reference.
    static Backend Create() { FakeGamepadApi::Reset(); return Backend(&FakeGamepadApi::Environment,reinterpret_cast<jobject>(1)); }
    /// Creates a physical event independent of the controller's SDK slot.
    static AInputEvent Controller(int deviceId,uint32_t buttons,bool motion=false,float x=0,float y=0) {
        AInputEvent event{}; event.Source=motion?AINPUT_SOURCE_JOYSTICK:AINPUT_SOURCE_GAMEPAD; event.Type=motion?AINPUT_EVENT_TYPE_MOTION:AINPUT_EVENT_TYPE_KEY;
        event.DeviceId=deviceId; event.Buttons=buttons; event.X=x; event.Y=y; return event;
    }
    /// Creates a keyboard transition for an existing navigation alias.
    static AInputEvent Key(int key,bool down,int repeat=0) { AInputEvent event{}; event.Key=key; event.Action=down?AKEY_EVENT_ACTION_DOWN:AKEY_EVENT_ACTION_UP; event.Repeat=repeat; return event; }
    /// Verifies explicit start/stop, one SDK update per frame, and callback retirement before destruction.
    static void Lifecycle() {
        {
            auto backend=Create(); Frame frame{};
            Check(backend.Capture(frame.data())==0,"No device must not fabricate a connected pad.");
            backend.SetStarted(true); backend.SetStarted(true); backend.SetStarted(false); backend.SetStarted(false);
            Check(FakeGamepadApi::Starts==1 && FakeGamepadApi::Stops==1,"Activity listener transitions must be balanced and idempotent.");
            Check(FakeGamepadApi::Updates==1,"Capture must update the SDK once.");
        }
        Check(!FakeGamepadApi::Initialized && !FakeGamepadApi::Callback && !FakeGamepadApi::Owner && FakeGamepadApi::Destroys==1,"SDK lifetime and callback owner must retire together.");
    }
    /// Preserves noncontiguous slots and all eight supported devices without merging their buttons.
    static void IndependentSlots() {
        auto backend=Create(); Frame frame{};
        FakeGamepadApi::Connect(1,400); FakeGamepadApi::Connect(7,900);
        auto first=Controller(400,PADDLEBOAT_BUTTON_A); auto last=Controller(900,PADDLEBOAT_BUTTON_B);
        backend.HandleEvent(&first); backend.HandleEvent(&last);
        Check(backend.Capture(frame.data())==8 && !frame[0].Connected && frame[1].Connected && frame[7].Connected,"Controller indices must preserve holes.");
        Check(frame[1].IsButtonDown(InputGamepadButton::South) && !frame[1].IsButtonDown(InputGamepadButton::East) && frame[7].IsButtonDown(InputGamepadButton::East),"Controller masks must remain independent.");
        for (int index=0;index<8;++index) FakeGamepadApi::Connect(index,1000+index);
        backend.Capture(frame.data());
        Check(backend.Capture(frame.data())==8,"All eight devices must be published.");
        for (const auto& state:frame) Check(state.Connected,"Each device must retain its own connected state.");
    }
    /// Covers every engine button, axis saturation, hardware flat regions, and invalid numeric input.
    static void ButtonsAndAxes() {
        auto backend=Create(); Frame frame{}; FakeGamepadApi::Connect(0,20);
        auto& data=FakeGamepadApi::Data[0]; data.buttonsDown=(1u<<17)-1;
        data.leftStick={2,-2}; data.rightStick={0.08f,std::numeric_limits<float>::quiet_NaN()}; data.triggerL2=2; data.triggerR2=-1;
        FakeGamepadApi::Info[0].rightStickPrecision.stickFlatX=0.1f;
        backend.Capture(frame.data());
        Check(frame[0].Buttons==((uint64_t{1}<<17)-1),"All supported SDK buttons must map to the engine ordering.");
        Check(frame[0].LeftStickX==32767 && frame[0].LeftStickY==-32767 && frame[0].RightStickX==0 && frame[0].RightStickY==0,"Axes must clamp and honor hardware flat regions.");
        Check(frame[0].LeftTrigger==32767 && frame[0].RightTrigger==0,"Triggers must use normalized L2/R2 rather than shoulder values.");
        data.triggerL2=0.5f; data.triggerR2=0.75f; data.triggerL1=data.triggerR1=1;
        backend.Capture(frame.data()); Check(frame[0].LeftTrigger==16384 && frame[0].RightTrigger==24575,"Analog trigger precision must be retained.");
    }
    /// A face-button key must never replace the physical stick with synthesized d-pad axes.
    static void AnalogSurvivesKey() {
        auto backend=Create(); Frame frame{}; FakeGamepadApi::Connect(0,20);
        auto motion=Controller(20,0,true,0.5f,-0.75f); backend.HandleEvent(&motion);
        auto press=Controller(20,PADDLEBOAT_BUTTON_A); backend.HandleEvent(&press); backend.Capture(frame.data());
        Check(frame[0].LeftStickX==16384 && frame[0].LeftStickY==-24575 && frame[0].IsButtonDown(InputGamepadButton::South),"Face-button input must preserve physical analog state.");
    }
    /// Physical down/up pairs drained before a frame still emit one press followed by release.
    static void QuickTap() {
        auto backend=Create(); Frame frame{}; FakeGamepadApi::Connect(3,70);
        auto down=Controller(70,PADDLEBOAT_BUTTON_X); auto up=Controller(70,0); backend.HandleEvent(&down); backend.HandleEvent(&up);
        backend.Capture(frame.data()); Check(frame[3].IsButtonDown(InputGamepadButton::West),"Quick physical taps must survive event draining.");
        backend.Capture(frame.data()); Check(frame[3].Buttons==0,"A latched tap must release on the next frame.");
    }
    /// Distinct aliases for one logical button retain their own held lifetimes and keyboard tap latch.
    static void KeyboardAliases() {
        auto backend=Create(); Frame frame{};
        auto w=Key(AKEYCODE_W,true); auto up=Key(AKEYCODE_DPAD_UP,true); auto releaseW=Key(AKEYCODE_W,false);
        backend.HandleEvent(&w); backend.HandleEvent(&up); backend.HandleEvent(&releaseW); backend.Capture(frame.data());
        Check(frame[0].LeftStickY==-32767 && frame[0].IsButtonDown(InputGamepadButton::DPadUp),"Releasing one keyboard alias must preserve another held alias.");
        auto releaseUp=Key(AKEYCODE_DPAD_UP,false); backend.HandleEvent(&releaseUp);
        auto enter=Key(AKEYCODE_ENTER,true); auto enterUp=Key(AKEYCODE_ENTER,false); backend.HandleEvent(&enter); backend.HandleEvent(&enterUp);
        backend.Capture(frame.data()); Check(frame[0].LeftStickY==0 && frame[0].IsButtonDown(InputGamepadButton::South),"Keyboard quick taps must be captured once.");
        backend.Capture(frame.data()); Check(frame[0].Buttons==0,"Keyboard tap latch must not stick.");
    }
    /// Keyboard aliases overlay player zero while keeping physical controls and other players intact.
    static void KeyboardAndController() {
        auto backend=Create(); Frame frame{}; FakeGamepadApi::Connect(0,20); FakeGamepadApi::Connect(2,30);
        auto physical=Controller(20,PADDLEBOAT_BUTTON_A,true,0.5f,0); backend.HandleEvent(&physical);
        auto key=Key(AKEYCODE_ENTER,true); auto release=Key(AKEYCODE_ENTER,false); backend.HandleEvent(&key); backend.HandleEvent(&release); backend.Capture(frame.data());
        Check(frame[0].LeftStickX==16384 && frame[0].IsButtonDown(InputGamepadButton::South) && frame[2].Connected,"Keyboard release must not clear held physical controls.");
        auto right=Key(AKEYCODE_D,true); backend.HandleEvent(&right); backend.Capture(frame.data()); Check(frame[0].LeftStickX==32767,"Keyboard navigation must remain usable with a controller connected.");
        right.Action=AKEY_EVENT_ACTION_UP; backend.HandleEvent(&right); backend.Capture(frame.data()); Check(frame[0].LeftStickX==16384,"Physical analog must return after keyboard navigation releases.");
    }
    /// Focus loss blocks held buttons and axes until neutral, then accepts newly pressed controls.
    static void FocusReset() {
        auto backend=Create(); Frame frame{}; FakeGamepadApi::Connect(0,20);
        auto held=Controller(20,PADDLEBOAT_BUTTON_A,true,0.7f,0); backend.HandleEvent(&held); backend.Capture(frame.data()); backend.Reset();
        backend.Capture(frame.data()); Check(frame[0].Connected && frame[0].Buttons==0 && frame[0].LeftStickX==0,"Cached held input must not reappear after focus loss.");
        auto extra=Controller(20,PADDLEBOAT_BUTTON_A|PADDLEBOAT_BUTTON_B); backend.HandleEvent(&extra); backend.Capture(frame.data());
        Check(!frame[0].IsButtonDown(InputGamepadButton::South) && frame[0].IsButtonDown(InputGamepadButton::East),"New buttons must work while previously held buttons remain blocked.");
        auto neutral=Controller(20,0,true,0,0); backend.HandleEvent(&neutral); backend.Capture(frame.data());
        backend.HandleEvent(&held); backend.Capture(frame.data()); Check(frame[0].IsButtonDown(InputGamepadButton::South) && frame[0].LeftStickX>0,"Physical controls must unlock after returning to neutral.");
    }
    /// Auto-repeat after focus loss does not invent a new key press; canceled events clear held aliases.
    static void KeyboardRepeatReset() {
        auto backend=Create(); Frame frame{}; auto key=Key(AKEYCODE_W,true); backend.HandleEvent(&key); backend.Reset();
        key.Repeat=4; backend.HandleEvent(&key); Check(backend.Capture(frame.data())==0,"Repeat events must not reactivate a reset keyboard.");
        key.Repeat=0; backend.HandleEvent(&key); backend.Capture(frame.data()); Check(frame[0].LeftStickY==-32767,"A new key-down must restore keyboard navigation.");
        key.Flags=AKEY_EVENT_FLAG_CANCELED; backend.HandleEvent(&key); backend.Capture(frame.data()); Check(frame[0].LeftStickY==0 && frame[0].Buttons==0,"Canceled key-down must release held state.");
    }
    /// Disconnect clears pending taps and gives a replacement one neutral frame without moving other slots.
    static void DisconnectAndReplacement() {
        auto backend=Create(); Frame frame{}; FakeGamepadApi::Connect(0,20); FakeGamepadApi::Connect(5,60);
        auto old=Controller(20,PADDLEBOAT_BUTTON_A); backend.HandleEvent(&old); backend.Capture(frame.data());
        FakeGamepadApi::Disconnect(0); FakeGamepadApi::Connect(0,21); auto replacement=Controller(21,PADDLEBOAT_BUTTON_A); backend.HandleEvent(&replacement);
        backend.Capture(frame.data()); Check(!frame[0].Connected && frame[0].Buttons==0 && frame[5].Connected,"Slot replacement must publish retirement without disturbing another player.");
        backend.Capture(frame.data()); Check(frame[0].Connected && frame[0].IsButtonDown(InputGamepadButton::South),"Replacement press must become a fresh action after retirement.");
        auto up=Controller(21,0); backend.HandleEvent(&up); FakeGamepadApi::Disconnect(0); backend.Capture(frame.data());
        Check(!frame[0].Connected && frame[0].Buttons==0 && frame[5].Connected,"Disconnect must release buttons while retaining surviving indices.");
        FakeGamepadApi::Connect(0,22); auto next=Controller(22,PADDLEBOAT_BUTTON_X); backend.HandleEvent(&next); backend.Capture(frame.data());
        FakeGamepadApi::Disconnect(0); FakeGamepadApi::Connect(0,23);
        auto quick=Controller(23,PADDLEBOAT_BUTTON_B); auto quickUp=Controller(23,0); backend.HandleEvent(&quick); backend.HandleEvent(&quickUp);
        backend.Capture(frame.data()); Check(!frame[0].Connected,"Replacement must retire even if its first tap is already released.");
        backend.Capture(frame.data()); Check(frame[0].Connected && frame[0].IsButtonDown(InputGamepadButton::East) && !frame[0].IsButtonDown(InputGamepadButton::West),"A replacement tap must survive delayed callbacks without retaining the old device mask.");
        backend.Capture(frame.data()); Check(frame[0].Buttons==0,"Replacement tap must release on its following frame.");
    }
    /// Mouse/touch remain unconsumed, unknown physical devices never become the keyboard fallback.
    static void PointerRouting() {
        auto backend=Create(); Frame frame{}; AInputEvent pointer{}; pointer.Type=AINPUT_EVENT_TYPE_MOTION; pointer.Source=AINPUT_SOURCE_MOUSE;
        Check(!backend.HandleEvent(&pointer),"Mouse events belong to the pointer adapter."); pointer.Source=AINPUT_SOURCE_TOUCHSCREEN; Check(!backend.HandleEvent(&pointer),"Touch events belong to the contact tracker.");
        auto unknown=Controller(200,PADDLEBOAT_BUTTON_A); unknown.Key=AKEYCODE_ENTER; Check(backend.HandleEvent(&unknown),"Undiscovered controller events must be isolated.");
        Check(backend.Capture(frame.data())==0,"Undiscovered controllers must not fabricate keyboard player zero.");
        FakeGamepadApi::Connect(4,40); auto mixed=Controller(40,PADDLEBOAT_BUTTON_B); mixed.Source=0x301; backend.HandleEvent(&mixed); backend.Capture(frame.data());
        Check(frame[4].IsButtonDown(InputGamepadButton::East) && !frame[0].Connected,"Keyboard-shaped controller events must retain their physical SDK slot.");
    }
    /// SDK failures propagate and a duplicate owner cannot destroy the live SDK registration.
    static void Failures() {
        auto backend=Create(); Frame frame{}; bool duplicate=false;
        try { Backend second(&FakeGamepadApi::Environment,reinterpret_cast<jobject>(1)); }
        catch (const std::runtime_error&) { duplicate=true; }
        Check(duplicate && FakeGamepadApi::Initialized && FakeGamepadApi::Destroys==0 && FakeGamepadApi::Owner==&backend,"Duplicate owner rejection must preserve the running adapter.");
        FakeGamepadApi::Connect(0,20); FakeGamepadApi::ReadResult=PADDLEBOAT_ERROR_NO_CONTROLLER; bool readFailed=false;
        try { backend.Capture(frame.data()); } catch (const std::runtime_error&) { readFailed=true; }
        Check(readFailed,"SDK read failures must propagate."); FakeGamepadApi::ReadResult=PADDLEBOAT_NO_ERROR;
        FakeGamepadApi::JavaException=true; bool javaFailed=false;
        try { backend.SetStarted(true); } catch (const std::runtime_error&) { javaFailed=true; }
        Check(javaFailed && !FakeGamepadApi::JavaException,"Java exceptions must be reported and cleared before unwinding.");
    }
};

/// Reports a failed scenario as a nonzero native test process exit code.
int main() { try { AndroidGamepadTests::Run(); return 0; } catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; } }
