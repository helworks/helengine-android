#include "AndroidRuntimeDiagnosticsProvider.hpp"
#include "RuntimeMemoryDiagnosticsSnapshot.hpp"
#include "RuntimeMemoryCounters.hpp"
#include "RuntimeDiagnosticsMetric.hpp"
#include <memory>
#include <stdexcept>

namespace helengine::android {
    void AndroidRuntimeDiagnosticsProvider::DestroySnapshot(::RuntimeMemoryDiagnosticsSnapshot* snapshot) {
        if (snapshot) { snapshot->Dispose(); delete snapshot; }
    }
    void AndroidRuntimeDiagnosticsProvider::AppendMetric(::RuntimeMemoryDiagnosticsSnapshot* snapshot,const char* name,uint64_t value) {
        auto metric=std::make_unique<::RuntimeDiagnosticsMetric>(name,value);
        snapshot->DetailMetrics->AddOwned(metric.get()); metric.release();
    }
    ::RuntimeMemoryDiagnosticsSnapshot* AndroidRuntimeDiagnosticsProvider::CaptureSnapshot() {
        const auto sample=Reader.Capture(true);
        std::unique_ptr<::RuntimeMemoryDiagnosticsSnapshot,decltype(&DestroySnapshot)> snapshot(new ::RuntimeMemoryDiagnosticsSnapshot(),DestroySnapshot);
        snapshot->ResidentBytes=sample.ResidentBytes; snapshot->PeakResidentBytes=sample.PeakResidentBytes; snapshot->PageFaultCount=sample.PageFaultCount;
        // Android virtual size and allocator arenas are not a measurement of whole-process committed memory.
        snapshot->CommittedBytes=0; snapshot->PeakCommittedBytes=0; snapshot->AvailablePhysicalBytes=0;
        AppendMetric(snapshot.get(),"android_process_virtual_bytes",sample.VirtualBytes);
        AppendMetric(snapshot.get(),"android_system_page_size_bytes",Reader.GetPageSize());
        AppendMetric(snapshot.get(),"android_native_heap_allocated_bytes",sample.NativeHeapAllocatedBytes);
        AppendMetric(snapshot.get(),"android_native_heap_free_bytes",sample.NativeHeapFreeBytes);
        AppendMetric(snapshot.get(),"android_native_heap_arena_bytes",sample.NativeHeapArenaBytes);
        AppendMetric(snapshot.get(),"android_native_heap_mapped_bytes",sample.NativeHeapMappedBytes);
        AppendMetric(snapshot.get(),"android_process_resident_is_estimate",1);
        AppendMetric(snapshot.get(),"android_thermal_status_available",Thermal.IsSupported()?1:0);
        if (Thermal.IsSupported()) AppendMetric(snapshot.get(),"android_thermal_status",static_cast<uint64_t>(Thermal.ReadStatus()));
        return snapshot.release();
    }
    void AndroidRuntimeDiagnosticsProvider::CaptureMemoryCounters(::RuntimeMemoryCounters* counters) {
        if (!counters) throw std::invalid_argument("Android memory sampling requires a reusable counter container.");
        const auto sample=Reader.Capture(false);
        counters->Reset(); counters->ResidentBytes=sample.ResidentBytes; counters->PeakResidentBytes=sample.PeakResidentBytes; counters->PageFaultCount=sample.PageFaultCount;
    }
}
