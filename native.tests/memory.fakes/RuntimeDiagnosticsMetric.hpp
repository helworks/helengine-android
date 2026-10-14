#pragma once
#include <string>
#include <cstdint>
/// Observes transient metric lifetime while retaining the actual engine constructor shape.
class RuntimeDiagnosticsMetric {
public:
    /// Counts live detail metrics to detect cleanup failures.
    inline static int Live=0;
    /// Stable metric identifier supplied by the native provider.
    std::string Name;
    /// Numeric metric value copied into the owned snapshot.
    uint64_t Value;
    /// Registers one live detail metric.
    RuntimeDiagnosticsMetric(std::string name,uint64_t value) : Name(name),Value(value) { ++Live; }
    /// Detects release of snapshot-owned and rejected metrics.
    ~RuntimeDiagnosticsMetric() { --Live; }
};
