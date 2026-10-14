#pragma once
#include "android/thermal.h"
#include <string>

/// Simulates OS capability, dynamic library functions and service failures for the actual thermal reader.
class FakeThermalApi {
public:
    /// Older OS versions must never attempt to resolve or acquire the API 30 service.
    static int ApiLevel;
    /// Stable current status returned by successful SDK queries.
    static AThermalStatus Status;
    /// Named missing symbol simulates an inconsistent supported runtime.
    static std::string MissingSymbol;
    /// Acquisition failures must not fabricate a NONE status or publish an owned handle.
    static bool AcquireFails;
    /// Observes function resolution, acquisition, release and query counts.
    static int Resolves,Acquires,Releases,Queries,LiveManagers;
    /// Resets only after all readers from the previous scenario have been destroyed.
    static void Reset(int apiLevel=26);
    /// Acquires one retained fake manager or reports failure.
    static AThermalManager* Acquire();
    /// Releases exactly one acquired manager.
    static void Release(AThermalManager* manager);
    /// Returns the current reported status, including injected SDK ERROR.
    static AThermalStatus Read(AThermalManager* manager);
};
