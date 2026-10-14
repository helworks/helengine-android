#pragma once
#include "RuntimeDiagnosticsMetric.hpp"
#include <memory>
#include <vector>
#include <new>
/// Simulates generated List.AddOwned and injected list growth failures.
class FakeDiagnosticsMetricList {
public:
    /// Nonnegative value injects a failure after this many successful insertions.
    inline static int FailAfter=-1;
    /// Owned metrics are released when the snapshot disposes its list.
    std::vector<std::unique_ptr<RuntimeDiagnosticsMetric>> Values;
    /// Transfers ownership only when insertion succeeds, matching the native list contract.
    void AddOwned(RuntimeDiagnosticsMetric* metric) {
        if (FailAfter>=0 && Values.size()>=static_cast<size_t>(FailAfter)) throw std::bad_alloc();
        Values.reserve(Values.size()+1); Values.emplace_back(metric);
    }
};
