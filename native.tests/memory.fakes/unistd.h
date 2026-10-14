#pragma once
#include <cstddef>
#include <cstdint>
constexpr int _SC_PAGESIZE=30;
/// Returns the fake device's actual page size or an injected failure.
long sysconf(int name);
/// Reads process counters in independently sized native chunks.
int64_t read(int descriptor,void* destination,size_t count);
/// Releases a process-counter descriptor exactly once.
int close(int descriptor);
