#pragma once

namespace helengine::android {
    /// Describes the ordered native-resolution and reduced-resolution passes used by a scene frame.
    enum class SceneFrameOperation {
        BeginPresentation, ClearCameraColor, EndPresentation, BeginWorld, DrawWorld,
        EndWorld, ResumePresentation, CompositeWorld, DrawOverlay
    };
}
