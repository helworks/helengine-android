using System.Text.Json;
using helengine.android.builder;
using helengine.baseplatform.Manifest;
using helengine.baseplatform.Profiles;
using helengine.baseplatform.Requests;
using helengine.editor;
using helengine.platforms;

namespace helengine.android.builder.tests;

/// <summary>
/// Exercises the real platform registry and editor loader, plus the scaffold's explicit export boundary.
/// </summary>
public sealed class AndroidPlatformIntegrationTests {
    /// <summary>
    /// Verifies a plugin registration can be discovered and turned into selectable editor profiles.
    /// </summary>
    [Fact]
    public void RegisteredPluginLoadsThroughEditorDiscovery() {
        string root = Path.Combine(AppContext.BaseDirectory, "registration-tests", Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(root);
        try {
            File.WriteAllText(Path.Combine(root, "platforms.json"), JsonSerializer.Serialize(new {
                platforms = new[] {
                    new {
                        engineVersion = "1.0.0+android-test",
                        platformId = "android",
                        displayName = "Android",
                        builderAssemblyPath = typeof(AndroidPlatformAssetBuilder).Assembly.Location,
                        playerSourceRootPath = AppContext.BaseDirectory,
                        pluginManifestPath = Path.Combine(AppContext.BaseDirectory, "platform-plugin.json")
                    }
                }
            }));

            AvailablePlatformProviderResolver resolver = new(new PlatformDiscoveryOptions(root));
            AvailablePlatformDescriptor descriptor = Assert.Single(resolver.LoadPlatforms("1.0.0+android-test"));
            Assert.True(descriptor.IsInstalled);
            Assert.Empty(resolver.LoadPlatforms("2.0.0"));
            var builder = new EditorPlatformAssetBuilderLoader().Load(descriptor.BuilderAssemblyPath);
            EditorPlatformBuildSelectionModel selection = EditorPlatformBuildSelectionModel.From(builder.Definition);
            Assert.Equal(descriptor.Id, selection.PlatformId);
            Assert.Equal(new[] { "debug", "release" }, selection.BuildProfiles.Select(profile => profile.ProfileId));
            Assert.All(selection.BuildProfiles, profile => {
                Assert.Contains(selection.GraphicsProfiles, graphics => graphics.ProfileId == profile.GraphicsProfileId);
                Assert.Contains(selection.CodegenProfiles, codegen => codegen.ProfileId == profile.CodegenProfileId);
            });
            Assert.Empty(selection.Definition.AssetCookCapabilities);
        } finally {
            Directory.Delete(root, true);
        }
    }

    /// <summary>
    /// Confirms the builder refuses to report an unfinished Android game export as successful.
    /// </summary>
    [Fact]
    public async Task GameExportFailsWithScaffoldDiagnostic() {
        PlatformBuildManifest manifest = new(
            1, "game", "1.0.0", "1.0.0", "android", "0.1.0", string.Empty,
            [], [], [], [], [], new PlatformContainerWritePlan(string.Empty, []), [],
            PlatformBuildRuntimeFeatureManifest.Empty);
        PlatformBuildRequest request = new(
            manifest,
            [new("android-native", "android", "android", "debug")],
            [new PlatformCookProfile("debug", "Debug", new PlatformCookProfileCapabilities(
                "android-native", "pending", "pending", "pending", PlatformSerializationEndianness.LittleEndian))],
            Path.Combine(AppContext.BaseDirectory, "unused-output"),
            Path.Combine(AppContext.BaseDirectory, "unused-working"));
        AndroidPlatformAssetBuilder builder = new();
        NotSupportedException error = await Assert.ThrowsAsync<NotSupportedException>(() => builder.BuildAsync(
            request, new AndroidBuildReporter(), new AndroidBuildReporter(), CancellationToken.None));
        Assert.Equal(AndroidPlatformAssetBuilder.ExportPendingMessage, error.Message);
        Assert.False(Directory.Exists(request.OutputRoot));
    }

    /// <summary>
    /// Confirms material cooking fails explicitly rather than emitting an unusable runtime payload.
    /// </summary>
    [Fact]
    public void MaterialCookingFailsWithScaffoldDiagnostic() {
        PlatformMaterialCookRequest request = new(
            "material", "materials/test.hmat", "android", "debug", "android-native", "", new Dictionary<string, string>());
        NotSupportedException error = Assert.Throws<NotSupportedException>(() => new AndroidPlatformAssetBuilder().CookMaterial(request));
        Assert.Equal(AndroidPlatformAssetBuilder.ExportPendingMessage, error.Message);
    }
}
