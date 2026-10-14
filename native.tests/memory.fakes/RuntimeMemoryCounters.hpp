#pragma once
#include <cstdint>
/// Scalar engine ABI fake also lets tests verify stale unknown fields are cleared.
class RuntimeMemoryCounters {
public:
    /// Current and peak process resident byte observations.
    uint64_t ResidentBytes=0,PeakResidentBytes=0;
    /// Committed and available physical memory remain unknown in this provider.
    uint64_t CommittedBytes=0,PeakCommittedBytes=0,AvailablePhysicalBytes=0;
    /// Process page-fault observation.
    uint64_t PageFaultCount=0;
    /// Clears every scalar just as the shared engine container does.
    void Reset() { ResidentBytes=PeakResidentBytes=CommittedBytes=PeakCommittedBytes=AvailablePhysicalBytes=PageFaultCount=0; }
};
