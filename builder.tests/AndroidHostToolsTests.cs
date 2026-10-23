using System.Text.RegularExpressions;
using helengine.android.builder;
using helengine.baseplatform.Manifest;
using helengine.baseplatform.Profiles;
using helengine.baseplatform.Requests;

namespace helengine.android.builder.tests;

/// <summary>Exercises the real host script and checked-in Gradle wrapper without installing an Android SDK.</summary>
[Collection("Android host tools")]
public sealed class AndroidHostToolsTests {
    /// <summary>Every pinned SDK tool must be found under the current host's official package layout.</summary>
    [Theory]
    [InlineData(0)]
    [InlineData(1)]
    [InlineData(2)]
    [InlineData(3)]
    [InlineData(4)]
    [InlineData(5)]
    public async Task MissingPinnedToolFailsBeforeGradle(int toolIndex) {
        using var fixture = new AndroidHostToolsFixture();
        string missing = fixture.RequiredTools[toolIndex];
        File.Delete(missing);
        var result = await fixture.RunScript();
        Assert.NotEqual(0, result.ExitCode);
        Assert.Contains("Required Android build tool is missing:", result.Output);
        Assert.Contains(Regex.Replace(missing, @"\s", ""), Regex.Replace(result.Output, @"\s", ""));
        Assert.False(File.Exists(fixture.TracePath));
    }

    /// <summary>The host portability change must not weaken the pinned JDK requirement.</summary>
    [Fact]
    public async Task DifferentJdkVersionIsRejected() {
        using var fixture = new AndroidHostToolsFixture();
        File.WriteAllText(Path.Combine(fixture.JavaRoot, "release"), "JAVA_VERSION=\"18.0.1\"");
        var result = await fixture.RunScript();
        Assert.NotEqual(0, result.ExitCode);
        Assert.Contains("JDK 17 is required", result.Output);
        Assert.False(File.Exists(fixture.TracePath));
    }

    /// <summary>The actual wrapper receives paths with spaces and a failing Java child cannot become a successful APK report.</summary>
    [Fact]
    public async Task OfficialWrapperPropagatesChildFailure() {
        using var fixture = new AndroidHostToolsFixture();
        var result = await fixture.RunScript();
        Assert.NotEqual(0, result.ExitCode);
        Assert.Contains("Android host build failed", result.Output);
        string[] arguments = File.ReadAllLines(fixture.TracePath);
        Assert.Equal(Path.Combine(fixture.Repository, "android", "gradle", "wrapper", "gradle-wrapper.jar"),
            Path.GetFullPath(Assert.Single(arguments, argument => argument.EndsWith("gradle-wrapper.jar", StringComparison.Ordinal))));
        Assert.Contains(":app:assembleDebug", arguments);
        Assert.Contains("--no-problems-report", arguments);
        Assert.Contains("-PhelengineBuildRoot=" + Path.Combine(fixture.NativeRoot, "app"), arguments);
        Assert.Contains("fixture-java-exit-37", File.ReadAllText(Path.Combine(fixture.WorkingRoot, "gradle.log")));
        Assert.False(File.Exists(Path.Combine(fixture.OutputRoot, "host-build-report.json")));
        Assert.Empty(Directory.GetFiles(fixture.OutputRoot, "*.apk"));
    }

    /// <summary>The builder starts the host PowerShell executable and preserves native child failure.</summary>
    [Fact]
    public async Task WorkspaceStartsHostPowerShellAndRejectsFailedApk() {
        using var fixture = new AndroidHostToolsFixture();
        fixture.SetWorkspaceEnvironment();
        Directory.CreateDirectory(Path.Combine(fixture.WorkingRoot, "apk-assets", "cooked"));
        string core = Path.Combine(fixture.Root, "generated core");
        Directory.CreateDirectory(core);
        File.WriteAllText(Path.Combine(core, "Core.hpp"), "// fixture: no native compilation occurs");
        PlatformBuildManifest manifest = new(1, "game", "1.0.0", "1.0.0", "android", "0.1.0", string.Empty,
            [], [], [], [], [], new PlatformContainerWritePlan(string.Empty, []), [], PlatformBuildRuntimeFeatureManifest.Empty);
        PlatformBuildRequest request = new(manifest, [new("android-native", "android", "android", "debug")],
            [new PlatformCookProfile("debug", "Debug", new PlatformCookProfileCapabilities("android-native", "pending", "pending", "pending", PlatformSerializationEndianness.LittleEndian))],
            fixture.OutputRoot, fixture.WorkingRoot, "debug", "android-native", "default", null, null, null, core);
        var reporter = new AndroidHostToolsProgressReporter();
        var error = await Assert.ThrowsAsync<InvalidOperationException>(() => AndroidBuildWorkspace.BuildAsync(request, reporter, CancellationToken.None));
        Assert.Contains("Android APK build failed", error.Message);
        Assert.True(File.Exists(fixture.TracePath));
        Assert.Single(reporter.Updates);
        Assert.False(File.Exists(Path.Combine(fixture.OutputRoot, "host-build-report.json")));
    }
}
