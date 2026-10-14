#pragma once
#include "FakeDiagnosticsMetricList.hpp"
/// Mirrors the generated snapshot's explicit Dispose ownership instead of hiding leaks in its destructor.
class RuntimeMemoryDiagnosticsSnapshot {
public:
    /// Counts live snapshot roots independently of their transient detail metrics.
    inline static int Live=0;
    /// Current and peak process resident bytes.
    uint64_t ResidentBytes=0,PeakResidentBytes=0;
    /// Common committed and available physical memory fields.
    uint64_t CommittedBytes=0,PeakCommittedBytes=0,AvailablePhysicalBytes=0;
    /// Process page-fault counter.
    uint64_t PageFaultCount=0;
    /// Snapshot owns detail metrics through explicit disposal.
    FakeDiagnosticsMetricList* DetailMetrics;
    /// Creates the detail graph just as the generated engine snapshot does.
    RuntimeMemoryDiagnosticsSnapshot() : DetailMetrics(new FakeDiagnosticsMetricList()) { ++Live; }
    /// Only releases the root; tests must observe explicit graph disposal.
    ~RuntimeMemoryDiagnosticsSnapshot() { --Live; }
    /// Releases all owned details deterministically.
    void Dispose() { delete DetailMetrics; DetailMetrics=nullptr; }
};
