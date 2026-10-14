#include "../src/platform/android/runtime/AndroidRuntimeDiagnosticsProvider.hpp"
#include "memory.fakes/FakeMemoryApi.hpp"
#include "RuntimeMemoryCounters.hpp"
#include "RuntimeMemoryDiagnosticsSnapshot.hpp"
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>

/// Exercises real native sampling and engine-provider code with deterministic OS and ownership boundaries.
class AndroidMemoryDiagnosticsTests {
public:
    /// Runs scalar, rich snapshot, numeric, native IO and failure-cleanup scenarios without Android.
    static void Run() {
        UnitsAndCounters(); RichSnapshot(); RepeatedScalarCapture(); InterruptedCalls(); MalformedCounters(); NativeFailures(); OverflowAndCleanup();
        std::cout<<"7 Android memory-diagnostics scenarios passed.\n";
    }
private:
    /// Fails a test with enough context to identify its violated observable contract.
    static void Check(bool condition,const char* message) { if (!condition) throw std::runtime_error(message); }
    /// Requires an operation to preserve explicit native or invalid-data failure behavior.
    static void Throws(const std::function<void()>& operation,const char* message) {
        bool failed=false; try { operation(); } catch (const std::exception&) { failed=true; } Check(failed,message);
    }
    /// Reads one rich snapshot metric by its stable public name.
    static uint64_t Metric(RuntimeMemoryDiagnosticsSnapshot* snapshot,const char* name) {
        for (const auto& value:snapshot->DetailMetrics->Values) if (value->Name==name) return value->Value;
        throw std::runtime_error("Expected Android detail metric is missing.");
    }
    /// Actual page width affects RSS and virtual size; kernel peak RSS always uses KiB independently.
    static void UnitsAndCounters() {
        for (const long pageSize:{4096,16384}) {
            FakeMemoryApi::Reset(); FakeMemoryApi::PageSize=pageSize;
            helengine::android::AndroidRuntimeDiagnosticsProvider provider; RuntimeMemoryCounters counters{};
            counters.CommittedBytes=counters.PeakCommittedBytes=counters.AvailablePhysicalBytes=999;
            provider.CaptureMemoryCounters(&counters);
            Check(counters.ResidentBytes==200*static_cast<uint64_t>(pageSize) && counters.PeakResidentBytes==1000*1024 && counters.PageFaultCount==15,"Process units must distinguish pages, KiB and fault counts.");
            Check(counters.CommittedBytes==0 && counters.PeakCommittedBytes==0 && counters.AvailablePhysicalBytes==0,"Unknown common fields must clear stale values instead of reporting virtual memory as committed.");
            Check(FakeMemoryApi::Handles==0 && FakeMemoryApi::Closes==1,"Sampling must close its process-counter descriptor.");
        }
    }
    /// Existing provider interfaces expose real scalar observations and allocator detail values with caller ownership.
    static void RichSnapshot() {
        FakeMemoryApi::Reset(); FakeMemoryApi::Heap.uordblks=uint64_t{5}*1024*1024*1024;
        helengine::android::AndroidRuntimeDiagnosticsProvider provider;
        auto* memoryProvider=dynamic_cast<IRuntimeMemoryCounterProvider*>(static_cast<IRuntimeDiagnosticsProvider*>(&provider));
        Check(memoryProvider!=nullptr,"Shared service must discover the reusable scalar interface on this provider.");
        auto* snapshot=provider.CaptureSnapshot();
        Check(snapshot->ResidentBytes==819200 && snapshot->PeakResidentBytes==1024000 && snapshot->PageFaultCount==15,"Snapshot scalars must match native observations.");
        Check(Metric(snapshot,"android_process_virtual_bytes")==4096000 && Metric(snapshot,"android_system_page_size_bytes")==4096,"Virtual memory must remain an explicitly named detail metric.");
        Check(Metric(snapshot,"android_native_heap_allocated_bytes")==uint64_t{5}*1024*1024*1024 && Metric(snapshot,"android_native_heap_free_bytes")==30000,"Bionic heap counters must retain values above 4 GiB.");
        Check(Metric(snapshot,"android_native_heap_arena_bytes")==80000 && Metric(snapshot,"android_native_heap_mapped_bytes")==12000 && Metric(snapshot,"android_process_resident_is_estimate")==1,"Heap detail kinds and approximate RSS must be explicit.");
        Check(FakeMemoryApi::HeapCalls==1 && RuntimeDiagnosticsMetric::Live==8,"Rich snapshot must own all memory details and the explicit thermal capability metric.");
        snapshot->Dispose(); delete snapshot; Check(RuntimeMemoryDiagnosticsSnapshot::Live==0 && RuntimeDiagnosticsMetric::Live==0,"Caller disposal must release the full snapshot graph.");
    }
    /// Reusable sampling observes current values without creating snapshots, metric lists or sampling the heap.
    static void RepeatedScalarCapture() {
        FakeMemoryApi::Reset(); helengine::android::AndroidRuntimeDiagnosticsProvider provider; RuntimeMemoryCounters counters{};
        for (int index=0;index<100;++index) {
            FakeMemoryApi::Statm="1000 "+std::to_string(200+index)+" 0 0 0 0 0\n";
            provider.CaptureMemoryCounters(&counters);
            Check(counters.ResidentBytes==static_cast<uint64_t>(200+index)*4096,"Scalar capture must observe newly reported RSS.");
        }
        Check(FakeMemoryApi::HeapCalls==0 && RuntimeMemoryDiagnosticsSnapshot::Live==0 && RuntimeDiagnosticsMetric::Live==0 && FakeMemoryApi::Handles==0,"Repeated sampling must avoid snapshot objects, heap detail calls and leaked descriptors.");
        Throws([&] { provider.CaptureMemoryCounters(nullptr); },"A missing reusable destination must fail explicitly.");
    }
    /// Partial native reads and interrupted open/read/close calls preserve exactly one descriptor retirement.
    static void InterruptedCalls() {
        FakeMemoryApi::Reset(); FakeMemoryApi::ReadLimit=2; FakeMemoryApi::InterruptOpen=FakeMemoryApi::InterruptRead=FakeMemoryApi::InterruptClose=true;
        helengine::android::AndroidProcessMemoryReader reader; const auto sample=reader.Capture(false);
        Check(sample.ResidentBytes==819200 && FakeMemoryApi::Closes==1 && FakeMemoryApi::Handles==0,"EINTR retry must preserve data without retrying Linux close.");
    }
    /// Strict unsigned parsing rejects corrupted records without inventing default memory numbers.
    static void MalformedCounters() {
        FakeMemoryApi::Reset(); helengine::android::AndroidProcessMemoryReader reader;
        for (const auto& record:{"","1000 200","1000 -200 0 0 0 0 0","1000 200x 0 0 0 0 0","1000 200 0 0 0 0 0 junk","18446744073709551616 200 0 0 0 0 0"}) {
            FakeMemoryApi::Statm=record; Throws([&] { reader.Capture(false); },"Malformed statm record must fail.");
            Check(FakeMemoryApi::Handles==0,"Parsing failures must close native descriptors.");
        }
        FakeMemoryApi::Statm=std::string(256,'1'); Throws([&] { reader.Capture(false); },"Oversized process records must fail within bounded storage.");
    }
    /// Native permission, read, close and resource-query failures propagate and leave no live descriptor.
    static void NativeFailures() {
        FakeMemoryApi::Reset(); FakeMemoryApi::PageSize=-1; Throws([] { helengine::android::AndroidProcessMemoryReader reader; },"Invalid system page size must fail.");
        FakeMemoryApi::Reset(); helengine::android::AndroidProcessMemoryReader reader;
        FakeMemoryApi::OpenFails=true; Throws([&] { reader.Capture(false); },"Native open failure must propagate."); Check(FakeMemoryApi::Handles==0,"Failed open must not create a descriptor.");
        FakeMemoryApi::OpenFails=false; FakeMemoryApi::ReadFails=true; Throws([&] { reader.Capture(false); },"Native read failure must propagate.");
        FakeMemoryApi::ReadFails=false; FakeMemoryApi::CloseFails=true; Throws([&] { reader.Capture(false); },"Native close failure must propagate.");
        FakeMemoryApi::CloseFails=false; FakeMemoryApi::UsageFails=true; Throws([&] { reader.Capture(false); },"Resource query failure must propagate.");
        Check(FakeMemoryApi::Handles==0,"Native failures must leave no live process-counter descriptor.");
        FakeMemoryApi::UsageFails=false; FakeMemoryApi::Usage.ru_maxrss=-1; Throws([&] { reader.Capture(false); },"Negative process counters must be rejected.");
    }
    /// Numeric conversion and injected metric insertion failures cannot leak the transient diagnostics graph.
    static void OverflowAndCleanup() {
        FakeMemoryApi::Reset(); helengine::android::AndroidProcessMemoryReader reader;
        FakeMemoryApi::Statm="18446744073709551615 200 0 0 0 0 0"; Throws([&] { reader.Capture(false); },"Page-to-byte overflow must fail.");
        FakeMemoryApi::Reset(); FakeMemoryApi::Usage.ru_maxrss=std::numeric_limits<int64_t>::max(); Throws([&] { reader.Capture(false); },"Peak KiB-to-byte overflow must fail.");
        FakeMemoryApi::Reset(); helengine::android::AndroidRuntimeDiagnosticsProvider provider;
        FakeDiagnosticsMetricList::FailAfter=2; Throws([&] { provider.CaptureSnapshot(); },"Metric insertion failure must propagate."); FakeDiagnosticsMetricList::FailAfter=-1;
        Check(RuntimeMemoryDiagnosticsSnapshot::Live==0 && RuntimeDiagnosticsMetric::Live==0 && FakeMemoryApi::Handles==0,"Failed snapshot construction must dispose all accepted and rejected metrics.");
    }
};
/// Exposes an observable native process exit status for every failed diagnostics scenario.
int main() { try { AndroidMemoryDiagnosticsTests::Run(); return 0; } catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; } }
