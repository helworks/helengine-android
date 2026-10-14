#pragma once
#include "sys/resource.h"
#include "malloc.h"
#include <string>

/// Injects deterministic process counters and Linux syscall failures into the production memory reader.
class FakeMemoryApi {
public:
    /// Native page width varies by simulated Android device.
    static long PageSize;
    /// Complete kernel statm record uses seven unsigned page counters.
    static std::string Statm;
    /// Current fake descriptor read position.
    static size_t Cursor;
    /// Native read limit exercises partial syscall reads.
    static size_t ReadLimit;
    /// Live descriptor count and close attempts expose leaks and unsafe EINTR retries.
    static int Handles,Closes,HeapCalls;
    /// Error injection for process-local native operations.
    static bool OpenFails,ReadFails,UsageFails,CloseFails;
    /// Single interrupted syscall attempts exercise retry behavior.
    static bool InterruptOpen,InterruptRead,InterruptClose;
    /// Peak RSS and fault counts returned for this process.
    static struct rusage Usage;
    /// Allocator detail values returned only when rich snapshots request them.
    static struct mallinfo Heap;
    /// Restores valid counters and clears every observation between test scenarios.
    static void Reset();
};
