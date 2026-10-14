#pragma once
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

/// Observes SDK calls and thread-local pairing without starting any real capture or Android process.
class FakeTraceApi {
public:
    /// System tracing can turn on or off while a scope already exists.
    static std::atomic<bool> Enabled;
    /// SDK call counts expose disabled-path overhead and mismatched ends.
    static std::atomic<int> Gates,Begins,Ends,InvalidEnds;
    /// Synchronous sections belong to their calling thread's stack.
    static thread_local std::vector<std::string> Stack;
    /// Event order remains observable after a scope has exited.
    static std::vector<std::string> Calls;
    /// Serializes fake call recording across test worker threads.
    static std::mutex Mutex;
    /// Clears all observations once preceding test scopes and workers have ended.
    static void Reset(bool enabled=false);
    /// Counts one named begin while preserving the current thread's parent section.
    static void Begin(const char* name);
    /// Counts the current thread's matching end or reports an invalid unmatched call.
    static void End();
};
