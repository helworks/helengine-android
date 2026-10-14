#pragma once

/// Supplies the planner's portable camera/world classification without a Vulkan device or Android activity.
struct ScenePlanTestBatch {
    /// Distinguishes world work from native overlays.
    bool WorldSpace;
    /// Starts a new camera even when no clear policy is enabled.
    bool BeginCamera;
    /// Requests a native-resolution backdrop clear.
    bool ClearColor;
    /// Retains the original depth policy on a world marker.
    bool ClearDepth;
};
