#pragma once
#include "IRuntimeDiagnosticsProvider.hpp"
#include "IRuntimeMemoryCounterProvider.hpp"
#include "AndroidProcessMemoryReader.hpp"
#include "../diagnostics/AndroidThermalStatusReader.hpp"

namespace helengine::android {
    /// Supplies Android memory and thermal observations through the shared snapshot and reusable-counter APIs.
    class AndroidRuntimeDiagnosticsProvider final : public ::IRuntimeDiagnosticsProvider,public ::IRuntimeMemoryCounterProvider {
    public:
        /// Transfers an owned snapshot with native details; the shared service appends scenes and engine collection metrics.
        ::RuntimeMemoryDiagnosticsSnapshot* CaptureSnapshot() override;
        /// Fills an existing scalar container without creating a snapshot, lists or heap detail metrics.
        void CaptureMemoryCounters(::RuntimeMemoryCounters* counters) override;
    private:
        /// Releases the complete snapshot graph when construction fails before ownership reaches the caller.
        static void DestroySnapshot(::RuntimeMemoryDiagnosticsSnapshot* snapshot);
        /// Appends one owned platform metric, keeping ownership local until the list accepts it.
        static void AppendMetric(::RuntimeMemoryDiagnosticsSnapshot* snapshot,const char* name,uint64_t value);
        /// Uses only native process-local counters and the actual system page size.
        AndroidProcessMemoryReader Reader;
        /// Samples thermal status only for rich snapshots; scalar memory sampling never opens this service.
        AndroidThermalStatusReader Thermal;
    };
}
