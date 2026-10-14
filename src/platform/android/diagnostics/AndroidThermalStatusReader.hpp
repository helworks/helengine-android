#pragma once
#include <android/thermal.h>
#include <mutex>

namespace helengine::android {
    /// Reads Android's reported thermal throttling status on supported OS versions without importing newer APIs.
    class AndroidThermalStatusReader final {
    public:
        /// Caches the actual runtime API level without acquiring a thermal service or registering a listener.
        AndroidThermalStatusReader();
        /// Releases the lazily acquired manager before the native host's activity and JVM attachment are retired.
        ~AndroidThermalStatusReader();
        /// Prevents copying the thermal manager's unique release responsibility.
        AndroidThermalStatusReader(const AndroidThermalStatusReader&)=delete;
        /// Prevents assigning another manager's service lifetime.
        AndroidThermalStatusReader& operator=(const AndroidThermalStatusReader&)=delete;
        /// Identifies the API 30 native contract; older OS versions must not be represented as a measured NONE status.
        bool IsSupported() const { return ApiLevel>=30; }
        /// Lazily opens the service and reads its current status, preserving native acquisition or query failures.
        int ReadStatus();
    private:
        /// Pointer signatures match the public SDK while avoiding direct references to API 30 symbols on API 26.
        using AcquireManagerFunction=AThermalManager*(*)();
        /// Releases an acquired SDK manager through the function resolved on a supported device.
        using ReleaseManagerFunction=void(*)(AThermalManager*);
        /// Reads the SDK enum without substituting a default on failure.
        using ReadStatusFunction=AThermalStatus(*)(AThermalManager*);
        /// Resolves all required functions before publishing a newly acquired owned manager.
        void OpenManager();
        /// Actual runtime OS version determines support independently of the APK compile SDK.
        int ApiLevel;
        /// Serializes lazy acquisition and reads when native callers request snapshots concurrently.
        std::mutex Mutex;
        /// Owned service handle remains absent until a supported rich snapshot first requests a status.
        AThermalManager* Manager=nullptr;
        /// Borrowed libandroid entry point releases the owned service handle exactly once.
        ReleaseManagerFunction ReleaseManager=nullptr;
        /// Borrowed libandroid entry point reads current framework status from the retained handle.
        ReadStatusFunction GetCurrentStatus=nullptr;
    };
}
