#pragma once
class RuntimeMemoryDiagnosticsSnapshot;
/// Mirrors the shared engine's provider ownership contract for isolated CPU tests.
class IRuntimeDiagnosticsProvider {
public:
    /// Providers support polymorphic native lifetime.
    virtual ~IRuntimeDiagnosticsProvider()=default;
    /// Caller receives an owned diagnostics snapshot.
    virtual RuntimeMemoryDiagnosticsSnapshot* CaptureSnapshot()=0;
};
