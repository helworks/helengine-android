#include "AndroidThermalStatusReader.hpp"
#include "AndroidTraceSection.hpp"
#include <android/api-level.h>
#include <dlfcn.h>
#include <stdexcept>

namespace helengine::android {
    AndroidThermalStatusReader::AndroidThermalStatusReader() : ApiLevel(android_get_device_api_level()) {
        if (ApiLevel<=0) throw std::runtime_error("Android thermal diagnostics require a valid runtime API level.");
    }
    AndroidThermalStatusReader::~AndroidThermalStatusReader() { if (Manager) ReleaseManager(Manager); }
    void AndroidThermalStatusReader::OpenManager() {
        const auto acquire=reinterpret_cast<AcquireManagerFunction>(dlsym(RTLD_DEFAULT,"AThermal_acquireManager"));
        const auto release=reinterpret_cast<ReleaseManagerFunction>(dlsym(RTLD_DEFAULT,"AThermal_releaseManager"));
        const auto read=reinterpret_cast<ReadStatusFunction>(dlsym(RTLD_DEFAULT,"AThermal_getCurrentThermalStatus"));
        if (!acquire || !release || !read) throw std::runtime_error("Supported Android thermal API is missing a required native entry point.");
        auto* candidate=acquire();
        if (!candidate) throw std::runtime_error("Android thermal manager acquisition failed.");
        Manager=candidate; ReleaseManager=release; GetCurrentStatus=read;
    }
    int AndroidThermalStatusReader::ReadStatus() {
        if (!IsSupported()) throw std::logic_error("Native Android thermal status is unavailable before API 30.");
        const AndroidTraceSection trace("HelEngine.Thermal.ReadStatus");
        std::lock_guard<std::mutex> lock(Mutex);
        if (!Manager) OpenManager();
        const auto status=GetCurrentStatus(Manager);
        if (status<0) throw std::runtime_error("Android thermal status query failed.");
        return static_cast<int>(status);
    }
}
