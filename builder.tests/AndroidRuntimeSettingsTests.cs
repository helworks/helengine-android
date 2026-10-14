using helengine.android.builder;

namespace helengine.android.builder.tests;

/// <summary>Verifies Android rendering contracts reach complete independent presets and portable native catalogs.</summary>
public sealed class AndroidRuntimeSettingsTests {
    /// <summary>Android exposes runtime scaling as a bounded number while retaining all common engine settings.</summary>
    [Fact]
    public void Definition_DeclaresMutableScaleAndCommonSettings() {
        var definition = AndroidPlatformDefinitionFactory.Create();
        var scale = Assert.Single(definition.RuntimeSettings, setting => setting.Id == AndroidRuntimeSettingIds.RenderScale);
        Assert.Equal(PlatformRuntimeSettingKind.Number, scale.Kind);
        Assert.Equal(1, scale.DefaultValue.Number);
        Assert.Equal(0.25, scale.Minimum);
        Assert.Equal(1, scale.Maximum);
        Assert.True(scale.RuntimeMutable);
        Assert.Contains(definition.RuntimeSettings, setting => setting.Id == CommonPlatformRuntimeSettings.ShadowsEnabled);
    }

    /// <summary>Every supported quality scale survives authoring and portable serialization without changing preset defaults.</summary>
    [Theory]
    [InlineData(0.25)]
    [InlineData(0.5)]
    [InlineData(0.75)]
    [InlineData(1)]
    public void Catalog_ExportsExplicitIndependentScaleValues(double value) {
        var definition = AndroidPlatformDefinitionFactory.Create();
        using var first = CreatePreset(definition.RuntimeSettings, "full", 1);
        using var second = CreatePreset(definition.RuntimeSettings, "custom", value);
        using var configuration = new PlatformSettingsConfiguration("android", definition.RuntimeSettings, [first, second], "custom");
        byte[] bytes = PlatformSettingsSerializer.WriteBytes(configuration);
        using var decoded = PlatformSettingsSerializer.ReadBytes(bytes);
        Assert.Equal(value, decoded.GetPreset("custom").Values.GetValue(AndroidRuntimeSettingIds.RenderScale).Number);
        Assert.Equal(1, decoded.GetPreset("full").Values.GetValue(AndroidRuntimeSettingIds.RenderScale).Number);
        Assert.Equal("custom", decoded.InitialPresetId);
    }

    /// <summary>Incomplete authored presets cannot silently inherit a new platform setting from another preset or default.</summary>
    [Fact]
    public void Catalog_RejectsMissingOrInvalidScale() {
        var definition = AndroidPlatformDefinitionFactory.Create();
        using var complete = CreatePreset(definition.RuntimeSettings, "quality", 1);
        var incompleteValues = complete.Values.CopyValues();
        incompleteValues.Remove(AndroidRuntimeSettingIds.RenderScale);
        using var incomplete = new PlatformSettingsPreset("quality", "Quality", incompleteValues);
        Assert.Throws<InvalidOperationException>(() => new PlatformSettingsConfiguration("android", definition.RuntimeSettings, [incomplete], "quality"));
        using var invalid = CreatePreset(definition.RuntimeSettings, "quality", double.NaN);
        Assert.Throws<InvalidOperationException>(() => new PlatformSettingsConfiguration("android", definition.RuntimeSettings, [invalid], "quality"));
    }

    /// <summary>Android exposes an explicit mutable integer FPS request, including display-rate mode.</summary>
    [Fact]
    public void Definition_DeclaresMutableTargetFrameRate() {
        var definition = AndroidPlatformDefinitionFactory.Create();
        var rate = Assert.Single(definition.RuntimeSettings, setting => setting.Id == AndroidRuntimeSettingIds.TargetFrameRate);
        Assert.Equal(PlatformRuntimeSettingKind.Integer, rate.Kind);
        Assert.Equal(0, rate.DefaultValue.Number);
        Assert.Equal(0, rate.Minimum);
        Assert.Equal(240, rate.Maximum);
        Assert.True(rate.RuntimeMutable);
    }

    /// <summary>Each preset retains its independently authored FPS value after portable native serialization.</summary>
    [Theory]
    [InlineData(0)]
    [InlineData(30)]
    [InlineData(60)]
    [InlineData(90)]
    [InlineData(120)]
    [InlineData(240)]
    public void Catalog_ExportsIndependentFrameRateValues(int framesPerSecond) {
        var definition = AndroidPlatformDefinitionFactory.Create();
        using var first = CreatePreset(definition.RuntimeSettings, "quality", 1);
        var values = first.Values.CopyValues();
        values[AndroidRuntimeSettingIds.TargetFrameRate] = PlatformSettingValue.FromInteger(framesPerSecond);
        using var second = new PlatformSettingsPreset("custom", "Custom", values);
        using var configuration = new PlatformSettingsConfiguration("android", definition.RuntimeSettings, [first, second], "custom");
        using var decoded = PlatformSettingsSerializer.ReadBytes(PlatformSettingsSerializer.WriteBytes(configuration));
        var rate = decoded.GetPreset("custom").Values.GetValue(AndroidRuntimeSettingIds.TargetFrameRate);
        Assert.Equal(PlatformRuntimeSettingKind.Integer, rate.Kind);
        Assert.Equal(framesPerSecond, rate.Number);
        Assert.Equal(0, decoded.GetPreset("quality").Values.GetValue(AndroidRuntimeSettingIds.TargetFrameRate).Number);
    }

    /// <summary>FPS must be explicitly authored in complete presets and use the exact integer contract and bounds.</summary>
    [Fact]
    public void Catalog_RejectsMissingOrInvalidFrameRate() {
        var definition = AndroidPlatformDefinitionFactory.Create();
        using var complete = CreatePreset(definition.RuntimeSettings, "quality", 1);
        var missingValues = complete.Values.CopyValues();
        missingValues.Remove(AndroidRuntimeSettingIds.TargetFrameRate);
        using var missing = new PlatformSettingsPreset("quality", "Quality", missingValues);
        Assert.Throws<InvalidOperationException>(() => new PlatformSettingsConfiguration("android", definition.RuntimeSettings, [missing], "quality"));
        foreach (var value in new[] { PlatformSettingValue.FromInteger(-1), PlatformSettingValue.FromInteger(241), PlatformSettingValue.FromNumber(60) }) {
            var invalidValues = complete.Values.CopyValues();
            invalidValues[AndroidRuntimeSettingIds.TargetFrameRate] = value;
            using var invalid = new PlatformSettingsPreset("quality", "Quality", invalidValues);
            Assert.Throws<InvalidOperationException>(() => new PlatformSettingsConfiguration("android", definition.RuntimeSettings, [invalid], "quality"));
        }
    }

    /// <summary>Builds a complete independently owned preset whose explicit scale replaces only its own creation value.</summary>
    static PlatformSettingsPreset CreatePreset(PlatformRuntimeSettingDefinition[] definitions, string id, double scale) {
        Dictionary<string, PlatformSettingValue> values = [];
        foreach (var definition in definitions) { values.Add(definition.Id, definition.DefaultValue); }
        values[AndroidRuntimeSettingIds.RenderScale] = PlatformSettingValue.FromNumber(scale);
        return new PlatformSettingsPreset(id, id, values);
    }
}
