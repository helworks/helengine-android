#pragma once
constexpr int O_RDONLY=0,O_CLOEXEC=0x80000;
/// Fake process-local open preserves Linux descriptor and EINTR behavior used by the real sampler.
int open(const char* path,int flags);
