using helengine.baseplatform.Builders;
using helengine.baseplatform.Reporting;

namespace helengine.android.builder.tests;

/// <summary>
/// Rejects unexpected build events because the scaffold should fail before producing content.
/// </summary>
public sealed class AndroidBuildReporter : IPlatformBuildProgressReporter, IPlatformBuildDiagnosticReporter {
    /// <summary>
    /// Fails a test if the scaffold starts reporting game-export progress.
    /// </summary>
    /// <param name="update">Unexpected progress event.</param>
    public void Report(PlatformBuildProgressUpdate update) {
        throw new InvalidOperationException("The scaffold must reject export before starting work.");
    }

    /// <summary>
    /// Fails a test if diagnostics are emitted instead of the explicit unsupported-operation error.
    /// </summary>
    /// <param name="diagnostic">Unexpected diagnostic event.</param>
    public void Report(PlatformBuildDiagnostic diagnostic) {
        throw new InvalidOperationException("The scaffold must reject export before starting work.");
    }
}
