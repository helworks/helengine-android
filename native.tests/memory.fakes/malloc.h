#pragma once
#ifdef _MSC_VER
#include <corecrt_malloc.h>
#endif
#include <cstddef>
/// Bionic uses size_t for its allocator counters, unlike the older glibc int ABI.
struct mallinfo {
    /// Allocator arena bytes acquired from the operating system.
    size_t arena=0;
    /// Bytes acquired through mapped allocator regions.
    size_t hblkhd=0;
    /// Current allocator-reported live bytes.
    size_t uordblks=0;
    /// Free bytes retained by the allocator.
    size_t fordblks=0;
};
/// Returns deterministic Bionic-shaped allocator observations.
struct mallinfo mallinfo();
