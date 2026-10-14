#include "FakeThermalApi.hpp"
#include "FakeThermalManager.hpp"
#include "dlfcn.h"
#include <cstring>
#include <stdexcept>

namespace {
    /// Deterministic opaque handle is retained only through observed acquire/release calls.
    AThermalManager Manager;
}
int FakeThermalApi::ApiLevel=26;
AThermalStatus FakeThermalApi::Status=ATHERMAL_STATUS_NONE;
std::string FakeThermalApi::MissingSymbol;
bool FakeThermalApi::AcquireFails=false;
int FakeThermalApi::Resolves=0,FakeThermalApi::Acquires=0,FakeThermalApi::Releases=0,FakeThermalApi::Queries=0,FakeThermalApi::LiveManagers=0;
void FakeThermalApi::Reset(int apiLevel) { ApiLevel=apiLevel; Status=ATHERMAL_STATUS_NONE; MissingSymbol.clear(); AcquireFails=false; Resolves=Acquires=Releases=Queries=LiveManagers=0; }
AThermalManager* FakeThermalApi::Acquire() { ++Acquires; if (AcquireFails) return nullptr; ++LiveManagers; return &Manager; }
void FakeThermalApi::Release(AThermalManager* manager) { if (manager!=&Manager || LiveManagers<=0) std::terminate(); ++Releases; --LiveManagers; }
AThermalStatus FakeThermalApi::Read(AThermalManager* manager) { if (manager!=&Manager || LiveManagers<=0) throw std::runtime_error("Thermal query used an unowned manager."); ++Queries; return Status; }
/// Returns the actual simulated runtime version.
int android_get_device_api_level() { return FakeThermalApi::ApiLevel; }
void* dlsym(void*,const char* name) {
    ++FakeThermalApi::Resolves;
    if (FakeThermalApi::MissingSymbol==name) return nullptr;
    if (std::strcmp(name,"AThermal_acquireManager")==0) return reinterpret_cast<void*>(&FakeThermalApi::Acquire);
    if (std::strcmp(name,"AThermal_releaseManager")==0) return reinterpret_cast<void*>(&FakeThermalApi::Release);
    if (std::strcmp(name,"AThermal_getCurrentThermalStatus")==0) return reinterpret_cast<void*>(&FakeThermalApi::Read);
    return nullptr;
}
