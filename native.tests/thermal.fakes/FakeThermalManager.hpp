#pragma once
/// Fake manager lifetime is observed through API counts; the production code never inspects these fields.
struct AThermalManager {
    /// Identifies the deterministic fake service handle.
    int Identity=1;
};
