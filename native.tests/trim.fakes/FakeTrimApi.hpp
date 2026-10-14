#pragma once
#include <jni.h>
#include <string>
#include <vector>

/// Exposes actual JNI identity semantics and observes fake Vulkan destruction calls in their real order.
class FakeTrimApi {
public:
    /// Minimal JNI table implements only identity checks used by successful native notifications.
    static JNIEnv Environment;
    /// Fake device bootstrap can represent lifecycle notifications before a logical device exists.
    static bool DeviceInitialized;
    /// Device-loss injection verifies trimming cannot free pending resources after a failed wait.
    static bool WaitFails;
    /// Ordered native calls expose wait/unmap/destroy/free behavior of the production cleanup.
    static std::vector<std::string> Calls;
    /// Restores valid bootstrap state and clears recorded driver calls between independent scenarios.
    static void Reset();
};
