#pragma once
#include <atomic>
/// Fake looper observes retained lifetime and background wakeups without an Android event thread.
struct ALooper {
    /// Registration holds one reference until callback retirement.
    std::atomic<int> References{0};
    /// Stronger queued notifications wake the native host.
    std::atomic<int> Wakes{0};
};
/// Retains the registration's looper reference.
void ALooper_acquire(ALooper* looper);
/// Retires the registration's looper reference.
void ALooper_release(ALooper* looper);
/// Observes a deferred wake request without executing renderer work on the callback thread.
void ALooper_wake(ALooper* looper);
