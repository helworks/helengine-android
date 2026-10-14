#pragma once
#include "AndroidProcessMemorySample.hpp"
#include <cstddef>

namespace helengine::android {
    /// Reads process-local Android/Linux memory counters through bounded native calls and stack storage.
    class AndroidProcessMemoryReader final {
    public:
        /// Reads the actual system page size once, supporting both 4 KB and 16 KB Android devices.
        AndroidProcessMemoryReader();
        /// Captures RSS, virtual size, peak RSS and page faults; optional heap details are allocator estimates.
        AndroidProcessMemorySample Capture(bool includeHeap) const;
        /// Returns the real page width used to convert process counters into bytes.
        uint64_t GetPageSize() const { return PageSize; }
    private:
        /// Reads all seven statm fields without accepting negative values, overflow or malformed tokens.
        static void ParseStatm(const char* data,size_t length,uint64_t* values);
        /// Converts page counts or KiB into bytes while rejecting integer overflow.
        static uint64_t Multiply(uint64_t count,uint64_t unit);
        /// Cached positive system page width remains immutable throughout this process.
        uint64_t PageSize;
    };
}
