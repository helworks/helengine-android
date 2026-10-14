#pragma once
constexpr int ANDROID_LOG_INFO=4;
/// Drops Android diagnostic output when running the real backend on a desktop CPU.
inline int __android_log_print(int,const char*,const char*,...) { return 0; }
