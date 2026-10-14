#include "FakeTraceApi.hpp"
#include "android/trace.h"

std::atomic<bool> FakeTraceApi::Enabled{false};
std::atomic<int> FakeTraceApi::Gates{0},FakeTraceApi::Begins{0},FakeTraceApi::Ends{0},FakeTraceApi::InvalidEnds{0};
thread_local std::vector<std::string> FakeTraceApi::Stack;
std::vector<std::string> FakeTraceApi::Calls;
std::mutex FakeTraceApi::Mutex;
void FakeTraceApi::Reset(bool enabled) { Enabled=enabled; Gates=Begins=Ends=InvalidEnds=0; Stack.clear(); Calls.clear(); }
void FakeTraceApi::Begin(const char* name) {
    Stack.emplace_back(name); ++Begins;
    std::lock_guard<std::mutex> lock(Mutex); Calls.push_back("B:"+Stack.back());
}
void FakeTraceApi::End() {
    ++Ends;
    if (Stack.empty()) { ++InvalidEnds; return; }
    { std::lock_guard<std::mutex> lock(Mutex); Calls.push_back("E:"+Stack.back()); }
    Stack.pop_back();
}
bool ATrace_isEnabled() { ++FakeTraceApi::Gates; return FakeTraceApi::Enabled; }
void ATrace_beginSection(const char* name) { FakeTraceApi::Begin(name); }
void ATrace_endSection() { FakeTraceApi::End(); }
