#pragma once
#include <cstdint>

namespace helengine::android {
    /// Holds one native process-memory observation without allocating a diagnostics object graph.
    struct AndroidProcessMemorySample {
        /// Estimated current resident set, converted from the kernel's process page counters.
        uint64_t ResidentBytes=0;
        /// Kernel high-water resident set reported by getrusage in KiB and converted to bytes.
        uint64_t PeakResidentBytes=0;
        /// Process virtual address space; this is not committed physical memory.
        uint64_t VirtualBytes=0;
        /// Sum of process minor and major page faults, excluding child processes.
        uint64_t PageFaultCount=0;
        /// Allocator-reported live native allocation bytes, sampled only for rich snapshots.
        uint64_t NativeHeapAllocatedBytes=0;
        /// Allocator-reported free native heap space retained by its arenas.
        uint64_t NativeHeapFreeBytes=0;
        /// Allocator-reported non-mapped arena bytes acquired from the operating system.
        uint64_t NativeHeapArenaBytes=0;
        /// Allocator-reported bytes in directly mapped heap regions.
        uint64_t NativeHeapMappedBytes=0;
    };
}
