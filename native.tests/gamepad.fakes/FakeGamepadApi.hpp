#pragma once
#include <paddleboat/paddleboat.h>
#include <array>

/// Deterministic SDK inventory and lifecycle observations shared by native adapter tests.
class FakeGamepadApi {
public:
    /// Actual JNI interface table with only the exception accessors used by this adapter.
    static JNIEnv Environment;
    /// SDK singleton lifetime, including duplicate initialization rejection.
    static bool Initialized;
    /// Injected pending Java exception.
    static bool JavaException;
    /// Tracks activity listener transitions and frame updates.
    static int Starts,Stops,Updates,Destroys,Processed;
    /// Error injection for SDK initialization and reads.
    static Paddleboat_ErrorCode InitResult,ReadResult;
    /// SDK controller statuses indexed independently of native device IDs.
    static std::array<Paddleboat_ControllerStatus,PADDLEBOAT_MAX_CONTROLLERS> Status;
    /// Standardized state returned by SDK reads.
    static std::array<Paddleboat_Controller_Data,PADDLEBOAT_MAX_CONTROLLERS> Data;
    /// Device IDs and hardware axis precision returned by SDK inventory.
    static std::array<Paddleboat_Controller_Info,PADDLEBOAT_MAX_CONTROLLERS> Info;
    /// Registered callback and borrowed backend owner.
    static Paddleboat_ControllerStatusCallback Callback;
    /// Context pointer retired before SDK teardown.
    static void* Owner;
    /// Slots with a pending disconnect callback, optionally followed by immediate replacement.
    static std::array<bool,PADDLEBOAT_MAX_CONTROLLERS> Disconnects;
    /// Restores a clean SDK and JNI environment between scenarios.
    static void Reset();
    /// Makes one controller visible in a chosen stable SDK slot.
    static void Connect(int index,int deviceId);
    /// Queues disconnection for the next frame update.
    static void Disconnect(int index);
};
