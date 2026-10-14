#pragma once
#include <cstdint>
constexpr int RUSAGE_SELF=0;
/// Minimal Linux resource ABI uses 64-bit fields even on the Windows test host.
struct rusage {
    /// Process resident high-water mark in KiB.
    int64_t ru_maxrss=0;
    /// Minor page faults handled without storage IO.
    int64_t ru_minflt=0;
    /// Major page faults requiring storage IO.
    int64_t ru_majflt=0;
};
/// Fills process resource counters or reports an injected failure.
int getrusage(int who,struct rusage* usage);
