#pragma once
#include <cstdint>

/// Native event shaped like the Android accessors consumed by the production adapter.
struct AInputEvent {
    /// Key or motion classification.
    int32_t Type=1;
    /// Android source bits; ordinary keyboard is the default.
    int32_t Source=0x101;
    /// Physical device identifier independent from the SDK slot.
    int32_t DeviceId=-1;
    /// Native key code used by keyboard navigation tests.
    int32_t Key=0;
    /// Key down/up action.
    int32_t Action=0;
    /// Native repeat count used to simulate held keys after focus loss.
    int32_t Repeat=0;
    /// Cancellation flag supplied by Android.
    int32_t Flags=0;
    /// Controller buttons injected into the fake SDK by an event.
    uint32_t Buttons=0;
    /// Controller left-stick position retained across face-button events.
    float X=0;
    /// Controller left-stick vertical position.
    float Y=0;
};
constexpr int AINPUT_EVENT_TYPE_KEY=1,AINPUT_EVENT_TYPE_MOTION=2;
constexpr int AINPUT_SOURCE_GAMEPAD=0x401,AINPUT_SOURCE_JOYSTICK=0x1000010;
constexpr int AINPUT_SOURCE_MOUSE=0x2002,AINPUT_SOURCE_TOUCHSCREEN=0x1002;
constexpr int AKEY_EVENT_ACTION_DOWN=0,AKEY_EVENT_ACTION_UP=1,AKEY_EVENT_FLAG_CANCELED=0x20;
/// Reads the native event classification.
inline int32_t AInputEvent_getType(const AInputEvent* event) { return event->Type; }
/// Reads source routing bits.
inline int32_t AInputEvent_getSource(const AInputEvent* event) { return event->Source; }
/// Reads the device ID used to match SDK inventory.
inline int32_t AInputEvent_getDeviceId(const AInputEvent* event) { return event->DeviceId; }
/// Reads a keyboard alias.
inline int32_t AKeyEvent_getKeyCode(const AInputEvent* event) { return event->Key; }
/// Reads a key transition.
inline int32_t AKeyEvent_getAction(const AInputEvent* event) { return event->Action; }
/// Reads repeated key-down count.
inline int32_t AKeyEvent_getRepeatCount(const AInputEvent* event) { return event->Repeat; }
/// Reads cancellation flags.
inline int32_t AKeyEvent_getFlags(const AInputEvent* event) { return event->Flags; }
