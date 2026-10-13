using helengine.baseplatform.Builders;
using helengine.baseplatform.Definitions;
using helengine.baseplatform.Descriptors;
using helengine.baseplatform.Reporting;
using helengine.baseplatform.Requests;
using helengine.baseplatform.Results;

namespace helengine.android.builder;

/// <summary>
/// Makes Android discoverable as an editor platform and explicitly rejects unfinished export operations.
/// </summary>
public sealed class AndroidPlatformAssetBuilder : IPlatformAssetBuilder {
    /// <summary>
    /// Explains the currently implemented milestone whenever a caller requests game export.
    /// </summary>
    public const string ExportPendingMessage = "Android game export is not implemented. This platform currently provides editor registration and a native host scaffold.";

    /// <summary>
    /// Initializes the metadata-only builder without requiring the Android SDK or NDK.
    /// </summary>
    public AndroidPlatformAssetBuilder() {
        Descriptor = new PlatformBuilderDescriptor(
            "helengine.android.builder", "0.1.0", "android",
            new EngineCompatibilityRange("1.0.0", "999.0.0"),
            new ManifestCompatibilityRange(1, 3),
            ["android"], ["debug", "release"]);
        Definition = AndroidPlatformDefinitionFactory.Create();
    }

    /// <summary>
    /// Gets the identity and compatibility bounds used by the editor's builder contract.
    /// </summary>
    public PlatformBuilderDescriptor Descriptor { get; }

    /// <summary>
    /// Gets Android's selectable build profiles and native scaffold metadata.
    /// </summary>
    public PlatformDefinition Definition { get; }

    /// <summary>
    /// Rejects material cooking until an Android rendering backend defines its runtime payloads.
    /// </summary>
    /// <param name="request">Material requested by the editor cook pipeline.</param>
    /// <returns>No cooked payload is produced by this scaffold.</returns>
    /// <exception cref="NotSupportedException">Android material cooking has not been implemented.</exception>
    public PlatformMaterialCookResult CookMaterial(PlatformMaterialCookRequest request) {
        ArgumentNullException.ThrowIfNull(request);
        throw new NotSupportedException(ExportPendingMessage);
    }

    /// <summary>
    /// Rejects game export so a scaffold cannot be mistaken for a successful Android package build.
    /// </summary>
    /// <param name="request">Resolved project build request.</param>
    /// <param name="progressReporter">Receiver for build progress.</param>
    /// <param name="diagnosticReporter">Receiver for build diagnostics.</param>
    /// <param name="cancellationToken">Cancellation requested by the editor.</param>
    /// <returns>A failed task explaining the pending exporter implementation.</returns>
    public Task<PlatformBuildReport> BuildAsync(
        PlatformBuildRequest request,
        IPlatformBuildProgressReporter progressReporter,
        IPlatformBuildDiagnosticReporter diagnosticReporter,
        CancellationToken cancellationToken) {
        ArgumentNullException.ThrowIfNull(request);
        ArgumentNullException.ThrowIfNull(progressReporter);
        ArgumentNullException.ThrowIfNull(diagnosticReporter);
        cancellationToken.ThrowIfCancellationRequested();
        return Task.FromException<PlatformBuildReport>(new NotSupportedException(ExportPendingMessage));
    }
}
