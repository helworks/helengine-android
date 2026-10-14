#pragma once
class RuntimeMemoryCounters;
/// Mirrors the engine's reusable scalar diagnostics interface.
class IRuntimeMemoryCounterProvider {
public:
    /// Providers support polymorphic native lifetime.
    virtual ~IRuntimeMemoryCounterProvider()=default;
    /// Existing counters are populated without snapshot creation.
    virtual void CaptureMemoryCounters(RuntimeMemoryCounters* counters)=0;
};
