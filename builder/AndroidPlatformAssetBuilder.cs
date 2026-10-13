using helengine.baseplatform.Builders;
using helengine.baseplatform.Definitions;
using helengine.baseplatform.Descriptors;
using helengine.baseplatform.Reporting;
using helengine.baseplatform.Requests;
using helengine.baseplatform.Results;

namespace helengine.android.builder;

/// <summary>
/// Registers Android and packages generated gameplay with the native Vulkan player.
/// </summary>
public sealed class AndroidPlatformAssetBuilder : IPlatformAssetBuilder {
    /// <summary>
    /// Initializes the metadata-only builder without requiring the Android SDK or NDK.
    /// </summary>
    public AndroidPlatformAssetBuilder() {
        Descriptor = new PlatformBuilderDescriptor(
            "helengine.android.builder", "0.1.0", "android",
            new EngineCompatibilityRange("1.0.0", "999.0.0"),
            new ManifestCompatibilityRange(1, 3),
            ["android"], ["debug"]);
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
    /// Cooks an untextured standard material into the Vulkan player's explicit color and lighting payload.
    /// </summary>
    /// <param name="request">Material requested by the editor cook pipeline.</param>
    /// <returns>Serialized platform material bytes with no authored shader dependency.</returns>
    /// <exception cref="NotSupportedException">The schema or textured material lies outside the current renderer capabilities.</exception>
    public PlatformMaterialCookResult CookMaterial(PlatformMaterialCookRequest request) {
        ArgumentNullException.ThrowIfNull(request);
        if (request.TargetPlatformId != "android" || request.SchemaId != "standard-shader") {
            throw new NotSupportedException("Android supports the standard-shader material schema only.");
        }
        string color = request.FieldValues.TryGetValue("base-color", out string value) ? value : "#ffffff";
        if (!color.StartsWith('#') || (color.Length != 7 && color.Length != 9)) {
            throw new InvalidOperationException("Android base color must use #RRGGBB or #RRGGBBAA.");
        }
        if (request.FieldValues.TryGetValue("texture-id", out string texture) && !string.IsNullOrWhiteSpace(texture)) {
            throw new NotSupportedException("Android 3D textured materials are not implemented yet.");
        }
        var material = new global::helengine.PlatformMaterialAsset {
            Id = request.MaterialAssetId, RendererFamilyId = "android-native", Lit = true,
            BaseColorR = Convert.ToByte(color.Substring(1, 2), 16),
            BaseColorG = Convert.ToByte(color.Substring(3, 2), 16),
            BaseColorB = Convert.ToByte(color.Substring(5, 2), 16),
            BaseColorA = color.Length == 9 ? Convert.ToByte(color.Substring(7, 2), 16) : (byte)255,
            TextureRelativePath = string.Empty
        };
        return new PlatformMaterialCookResult(global::helengine.files.AssetSerializer.SerializeToBytes(material), []);
    }

    /// <summary>
    /// Packages cooked authored content with the generated native runtime and verifies the resulting debug APK.
    /// </summary>
    /// <param name="request">Resolved project build request.</param>
    /// <param name="progressReporter">Receiver for build progress.</param>
    /// <param name="diagnosticReporter">Receiver for build diagnostics.</param>
    /// <param name="cancellationToken">Cancellation requested by the editor.</param>
    /// <returns>Successful report only after native compilation, signing, asset packaging, and alignment checks.</returns>
    public Task<PlatformBuildReport> BuildAsync(
        PlatformBuildRequest request,
        IPlatformBuildProgressReporter progressReporter,
        IPlatformBuildDiagnosticReporter diagnosticReporter,
        CancellationToken cancellationToken) {
        ArgumentNullException.ThrowIfNull(request);
        ArgumentNullException.ThrowIfNull(progressReporter);
        ArgumentNullException.ThrowIfNull(diagnosticReporter);
        cancellationToken.ThrowIfCancellationRequested();
        return AndroidBuildWorkspace.BuildAsync(request, progressReporter, cancellationToken);
    }
}
