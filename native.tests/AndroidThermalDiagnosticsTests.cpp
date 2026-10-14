#include "../src/platform/android/diagnostics/AndroidThermalStatusReader.hpp"
#include "../src/platform/android/runtime/AndroidRuntimeDiagnosticsProvider.hpp"
#include "thermal.fakes/FakeThermalApi.hpp"
#include "memory.fakes/FakeMemoryApi.hpp"
#include "RuntimeMemoryCounters.hpp"
#include "RuntimeMemoryDiagnosticsSnapshot.hpp"
#include <functional>
#include <iostream>
#include <stdexcept>
#include <thread>

/// Exercises actual runtime capability, service lifetime and diagnostics integration without Android or heat generation.
class AndroidThermalDiagnosticsTests {
public:
    /// Runs unsupported OS, valid states, failure, concurrency and shared snapshot scenarios.
    static void Run() {
        OlderOs(); LazyLifetimeAndStatuses(); MissingSymbols(); AcquisitionFailure(); QueryFailure(); ConcurrentReads(); SnapshotCapability(); SnapshotFailure(); ScalarMemoryRemainsIndependent();
        std::cout<<"9 Android thermal diagnostics scenarios passed.\n";
    }
private:
    /// Native reader under test owns service acquisition while gameplay still owns every quality decision.
    using Reader=helengine::android::AndroidThermalStatusReader;
    /// Fails an observable contract without relying on disabled release assertions.
    static void Check(bool condition,const char* message) { if (!condition) throw std::runtime_error(message); }
    /// Requires unsupported operations and SDK failures to remain explicit.
    static void Throws(const std::function<void()>& operation,const char* message) {
        bool failed=false; try { operation(); } catch (const std::exception&) { failed=true; } Check(failed,message);
    }
    /// Tests whether a snapshot publishes a stable platform metric.
    static bool HasMetric(RuntimeMemoryDiagnosticsSnapshot* snapshot,const char* name) {
        for (const auto& metric:snapshot->DetailMetrics->Values) if (metric->Name==name) return true;
        return false;
    }
    /// Reads a published metric without silently fabricating an absent value.
    static uint64_t Metric(RuntimeMemoryDiagnosticsSnapshot* snapshot,const char* name) {
        for (const auto& metric:snapshot->DetailMetrics->Values) if (metric->Name==name) return metric->Value;
        throw std::runtime_error("Thermal diagnostics metric is missing.");
    }
    /// Older OS versions expose lack of support while performing no dynamic symbol lookup or SDK acquisition.
    static void OlderOs() {
        for (int apiLevel:{26,27,28,29}) {
            FakeThermalApi::Reset(apiLevel); Reader reader;
            Check(!reader.IsSupported(),"Native thermal status must not claim support before API 30.");
            Throws([&] { reader.ReadStatus(); },"Unsupported status reads must fail rather than return NONE.");
            Check(FakeThermalApi::Resolves==0 && FakeThermalApi::Acquires==0 && FakeThermalApi::Queries==0,"Older devices must never touch new SDK symbols.");
        }
        FakeThermalApi::Reset(0); Throws([] { Reader reader; },"Unknown runtime API level must fail explicitly.");
    }
    /// One manager is acquired on demand, reports all seven current SDK statuses and releases exactly once.
    static void LazyLifetimeAndStatuses() {
        FakeThermalApi::Reset(30);
        { Reader reader; Check(reader.IsSupported() && FakeThermalApi::Resolves==0 && FakeThermalApi::Acquires==0,"Constructing diagnostics must not start a thermal service query.");
            for (int status=0;status<=6;++status) { FakeThermalApi::Status=static_cast<AThermalStatus>(status); Check(reader.ReadStatus()==status,"Every reported Android throttling level must remain unchanged."); }
            Check(FakeThermalApi::Resolves==3 && FakeThermalApi::Acquires==1 && FakeThermalApi::LiveManagers==1,"Repeated reads must reuse one valid service handle.");
        }
        Check(FakeThermalApi::Releases==1 && FakeThermalApi::LiveManagers==0,"Reader destruction must release its manager exactly once.");
    }
    /// Supported runtimes with missing required SDK functions fail before acquiring a handle that could leak.
    static void MissingSymbols() {
        for (const auto& name:{"AThermal_acquireManager","AThermal_releaseManager","AThermal_getCurrentThermalStatus"}) {
            FakeThermalApi::Reset(35); FakeThermalApi::MissingSymbol=name; Reader reader;
            Throws([&] { reader.ReadStatus(); },"Missing supported entry point must fail explicitly.");
            Check(FakeThermalApi::Acquires==0 && FakeThermalApi::Releases==0 && FakeThermalApi::LiveManagers==0,"All required entry points must resolve before service ownership is acquired.");
        }
    }
    /// Failed acquisition retains no handle or invented observation, and a later caller can retry explicitly.
    static void AcquisitionFailure() {
        FakeThermalApi::Reset(35);
        { Reader reader; FakeThermalApi::AcquireFails=true; Throws([&] { reader.ReadStatus(); },"Null SDK manager must fail.");
            Check(FakeThermalApi::LiveManagers==0 && FakeThermalApi::Releases==0,"Failed acquire must not publish ownership.");
            FakeThermalApi::AcquireFails=false; FakeThermalApi::Status=ATHERMAL_STATUS_MODERATE;
            Check(reader.ReadStatus()==2 && FakeThermalApi::Acquires==2,"A caller-requested retry must acquire the service before returning an actual observation.");
        }
        Check(FakeThermalApi::Releases==1 && FakeThermalApi::LiveManagers==0,"A successful retry must retain normal release responsibility.");
    }
    /// SDK ERROR is not converted to NONE or a large unsigned metric; valid later reads still use the owned service.
    static void QueryFailure() {
        FakeThermalApi::Reset(35);
        { Reader reader; FakeThermalApi::Status=ATHERMAL_STATUS_ERROR; Throws([&] { reader.ReadStatus(); },"SDK ERROR must remain an explicit query failure.");
            FakeThermalApi::Status=ATHERMAL_STATUS_SEVERE; Check(reader.ReadStatus()==3 && FakeThermalApi::Acquires==1,"A query failure must not invalidate otherwise live manager ownership.");
        }
        Check(FakeThermalApi::LiveManagers==0 && FakeThermalApi::Releases==1,"Error paths must still release their owned service.");
    }
    /// Simulates independent native callers requesting the same reader concurrently.
    static void ReadWorker(Reader* reader) { for (int index=0;index<100;++index) Check(reader->ReadStatus()==0,"Concurrent status read returned an unexpected observation."); }
    /// Lazy service acquisition is serialized so concurrent reads cannot leak or overwrite another owned manager.
    static void ConcurrentReads() {
        FakeThermalApi::Reset(35);
        { Reader reader; std::thread first(ReadWorker,&reader),second(ReadWorker,&reader); first.join(); second.join();
            Check(FakeThermalApi::Acquires==1 && FakeThermalApi::Queries==200,"Concurrent readers must share one protected manager lifetime.");
        }
        Check(FakeThermalApi::Releases==1 && FakeThermalApi::LiveManagers==0,"Concurrent reads must preserve a single release responsibility.");
    }
    /// Shared diagnostics distinguish measured NONE on supported devices from unsupported capability on older devices.
    static void SnapshotCapability() {
        for (int apiLevel:{26,35}) {
            FakeThermalApi::Reset(apiLevel); FakeMemoryApi::Reset(); helengine::android::AndroidRuntimeDiagnosticsProvider provider;
            auto* snapshot=provider.CaptureSnapshot();
            Check(snapshot->ResidentBytes==819200 && Metric(snapshot,"android_thermal_status_available")==static_cast<uint64_t>(apiLevel>=30),"Thermal details must preserve existing native memory counters and explicit capability.");
            if (apiLevel>=30) Check(HasMetric(snapshot,"android_thermal_status") && Metric(snapshot,"android_thermal_status")==0,"Supported NONE is a real observation.");
            else Check(!HasMetric(snapshot,"android_thermal_status") && FakeThermalApi::Resolves==0,"Unsupported snapshots must not fabricate a thermal status.");
            snapshot->Dispose(); delete snapshot;
            Check(RuntimeMemoryDiagnosticsSnapshot::Live==0 && RuntimeDiagnosticsMetric::Live==0,"Snapshot ownership must include the new detail metrics.");
        }
    }
    /// A failing thermal read disposes the already assembled memory snapshot instead of returning partial success.
    static void SnapshotFailure() {
        FakeThermalApi::Reset(35); FakeMemoryApi::Reset(); FakeThermalApi::Status=ATHERMAL_STATUS_ERROR;
        { helengine::android::AndroidRuntimeDiagnosticsProvider provider;
            Throws([&] { provider.CaptureSnapshot(); },"Thermal snapshot failure must propagate.");
            Check(RuntimeMemoryDiagnosticsSnapshot::Live==0 && RuntimeDiagnosticsMetric::Live==0 && FakeMemoryApi::Handles==0,"Failed rich capture must dispose its transient memory graph and native descriptors.");
        }
        Check(FakeThermalApi::LiveManagers==0,"Provider destruction must retire the thermal handle after a failed capture.");
    }
    /// Reusable memory sampling never resolves symbols, opens the thermal service or allocates rich metrics.
    static void ScalarMemoryRemainsIndependent() {
        FakeThermalApi::Reset(35); FakeMemoryApi::Reset(); FakeThermalApi::AcquireFails=true;
        helengine::android::AndroidRuntimeDiagnosticsProvider provider; RuntimeMemoryCounters counters;
        for (int index=0;index<100;++index) provider.CaptureMemoryCounters(&counters);
        Check(counters.ResidentBytes==819200 && FakeThermalApi::Resolves==0 && FakeThermalApi::Acquires==0 && FakeThermalApi::Queries==0 && RuntimeMemoryDiagnosticsSnapshot::Live==0,"Scalar memory capture must remain independent from thermal support and service failures.");
    }
};
/// Reports any failed capability, lifetime or diagnostics contract through a nonzero native exit status.
int main() { try { AndroidThermalDiagnosticsTests::Run(); return 0; } catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; } }
