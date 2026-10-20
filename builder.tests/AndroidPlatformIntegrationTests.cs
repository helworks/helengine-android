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
            Assert.Equal(new[] { "debug" }, selection.BuildProfiles.Select(profile => profile.ProfileId));
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
    public async Task GameExportRequiresGeneratedCoreBeforeWritingOutputs() {
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
            Path.Combine(AppContext.BaseDirectory, "unused-working"), "debug", "android-native", "default", null, null, null);
        AndroidPlatformAssetBuilder builder = new();
        InvalidOperationException error = await Assert.ThrowsAsync<InvalidOperationException>(() => builder.BuildAsync(
            request, new AndroidBuildReporter(), new AndroidBuildReporter(), CancellationToken.None));
        Assert.Contains("editor-generated native core", error.Message);
        Assert.False(Directory.Exists(request.OutputRoot));
    }

    /// <summary>
    /// Confirms material cooking fails explicitly rather than emitting an unusable runtime payload.
    /// </summary>
    [Fact]
    public void MaterialCookingRejectsUnsupportedSchemas() {
        PlatformMaterialCookRequest request = new(
            "material", "materials/test.hmat", "android", "debug", "android-native", "", new Dictionary<string, string>(), numericPolicy: global::helengine.NumericPrecisionPolicy.Default);
        NotSupportedException error = Assert.Throws<NotSupportedException>(() => new AndroidPlatformAssetBuilder().CookMaterial(request));
        Assert.Contains("standard-shader", error.Message);
    }

    /// <summary>Proves authored color bytes survive platform cooking and shader artifacts are not requested by the fixed Vulkan standard path.</summary>
    [Fact]
    public void MaterialCookingPreservesAuthoredColor() {
        var request = new PlatformMaterialCookRequest("material", "materials/test.hmat", "android", "debug", "android-native", "standard-shader",
            new Dictionary<string, string> { ["base-color"] = "#12345678" }, numericPolicy: global::helengine.NumericPrecisionPolicy.Default);
        var result = new AndroidPlatformAssetBuilder().CookMaterial(request);
        using var stream = new MemoryStream(result.CookedMaterialBytes);
        var asset = Assert.IsType<global::helengine.PlatformMaterialAsset>(global::helengine.files.AssetSerializer.Deserialize(stream));
        Assert.Equal("android-native", asset.RendererFamilyId);
        Assert.Equal(0x12, asset.BaseColorR);
        Assert.Equal(0x34, asset.BaseColorG);
        Assert.Equal(0x56, asset.BaseColorB);
        Assert.Equal(0x78, asset.BaseColorA);
        Assert.True(asset.Lit);
        Assert.Empty(result.ReferencedShaderDependencies);
    }

    /// <summary>A material keeps the editor's cooked dependency path instead of an authored texture id or a staging-only generated prefix.</summary>
    [Theory]
    [InlineData("cooked/textures/checker.hetex")]
    [InlineData("generated/cooked/textures/checker.hetex")]
    public void MaterialCookingUsesPackagedTextureDependency(string cookedPath) {
        var request = new PlatformMaterialCookRequest("material", "materials/checker.hmat", "android", "debug", "android-native", "standard-shader",
            new Dictionary<string, string> { ["texture-id"] = "checker", ["texture-relative-path"] = cookedPath }, numericPolicy: global::helengine.NumericPrecisionPolicy.Default);
        var result = new AndroidPlatformAssetBuilder().CookMaterial(request);
        using var stream = new MemoryStream(result.CookedMaterialBytes);
        var asset = Assert.IsType<global::helengine.PlatformMaterialAsset>(global::helengine.files.AssetSerializer.Deserialize(stream));
        Assert.Equal("cooked/textures/checker.hetex", asset.TextureRelativePath);
    }

    /// <summary>Unresolved texture identities and paths escaping cooked content fail before packaging.</summary>
    [Theory]
    [InlineData("")]
    [InlineData("cooked/../private.hetex")]
    [InlineData("C:/assets/checker.hetex")]
    public void MaterialCookingRejectsUnresolvedOrEscapingTextures(string cookedPath) {
        var request = new PlatformMaterialCookRequest("material", "materials/checker.hmat", "android", "debug", "android-native", "standard-shader",
            new Dictionary<string, string> { ["texture-id"] = "checker", ["texture-relative-path"] = cookedPath }, numericPolicy: global::helengine.NumericPrecisionPolicy.Default);
        Assert.Throws<InvalidOperationException>(() => new AndroidPlatformAssetBuilder().CookMaterial(request));
    }

    /// <summary>Shared dependency readers consume the generic record while the Android renderer receives exact authored PBR parameters and roughness identity.</summary>
    [Fact]
    public void MaterialCookingPreservesVersionedShadingParameters() {
        var request = new PlatformMaterialCookRequest("metal", "materials/metal.hmat", "android", "debug", "android-native", "standard-shader",
            new Dictionary<string, string> { ["metallic"] = "1", ["roughness"] = "0.25", ["specular"] = "0.75",
                ["casts-shadows"] = "false", ["receives-shadows"] = "true", ["roughness-texture-id"] = "roughness-atlas", ["alpha-mode"] = "alpha-test" }, numericPolicy: global::helengine.NumericPrecisionPolicy.Default);
        using var stream = new MemoryStream(new AndroidPlatformAssetBuilder().CookMaterial(request).CookedMaterialBytes);
        Assert.IsType<global::helengine.PlatformMaterialAsset>(global::helengine.files.AssetSerializer.Deserialize(stream));
        using var reader = global::helengine.EngineBinaryReader.Create(stream, global::helengine.EngineBinaryEndianness.LittleEndian);
        Assert.Equal(AndroidMaterialParametersSerializer.Magic, reader.ReadUInt32());
        Assert.Equal(AndroidMaterialParametersSerializer.Version, reader.ReadByte());
        Assert.Equal(1f, reader.ReadSingle()); Assert.Equal(0.25f, reader.ReadSingle()); Assert.Equal(0.75f, reader.ReadSingle());
        Assert.Equal(0, reader.ReadByte()); Assert.Equal(1, reader.ReadByte()); Assert.Equal(1, reader.ReadByte());
        Assert.Equal("cooked/imported/roughness-atlas", reader.ReadString());
        Assert.Equal(stream.Length, stream.Position);
    }

    /// <summary>Invalid surface coefficients fail at cook time rather than entering Vulkan shader calculations.</summary>
    [Theory]
    [InlineData("NaN")]
    [InlineData("-0.1")]
    [InlineData("1.1")]
    public void MaterialCookingRejectsInvalidShadingParameters(string value) {
        var request = new PlatformMaterialCookRequest("material", "materials/test.hmat", "android", "debug", "android-native", "standard-shader",
            new Dictionary<string, string> { ["roughness"] = value }, numericPolicy: global::helengine.NumericPrecisionPolicy.Default);
        Assert.Throws<InvalidOperationException>(() => new AndroidPlatformAssetBuilder().CookMaterial(request));
    }
}
