using System.Diagnostics;
using System.Text.Json;
using helengine.baseplatform.Builders;
using helengine.baseplatform.Reporting;
using helengine.baseplatform.Requests;

namespace helengine.android.builder;

/// <summary>Packages editor-cooked content together with its generated native runtime in a debug APK.</summary>
public static class AndroidBuildWorkspace {
    /// <summary>Stages only packaged artifacts, invokes the reproducible host build, and reports success after APK verification.</summary>
    public static async Task<PlatformBuildReport> BuildAsync(PlatformBuildRequest request,
        IPlatformBuildProgressReporter progress, CancellationToken cancellationToken) {
        if (!string.Equals(request.SelectedBuildProfileId, "debug", StringComparison.OrdinalIgnoreCase)) {
            throw new NotSupportedException($"Android game packaging currently supports debug APKs only; requested profile='{request.SelectedBuildProfileId}'.");
        }
        if (string.IsNullOrWhiteSpace(request.GeneratedCoreCppRootPath) || !File.Exists(Path.Combine(request.GeneratedCoreCppRootPath, "Core.hpp"))) {
            throw new InvalidOperationException("Android export requires the editor-generated native core.");
        }
        string repository = ResolveRepositoryRoot(Environment.GetEnvironmentVariable("HELENGINE_ANDROID_REPOSITORY_ROOT"), typeof(AndroidBuildWorkspace).Assembly.Location);
        if (!File.Exists(Path.Combine(repository, "scripts", "build-host.ps1"))) {
            throw new InvalidOperationException("Publish the Android builder to its platform repository artifacts/builder directory.");
        }
        string generated = Path.GetFullPath(request.GeneratedCoreCppRootPath);
        if (!File.Exists(Path.Combine(generated, "Core.hpp"))) {
            throw new InvalidOperationException("Android export requires the editor-generated native core.");
        }
        string packaged = Path.Combine(request.WorkingRoot, "apk-assets");
        Directory.CreateDirectory(packaged);
        string staging = Directory.GetCurrentDirectory();
        foreach (var artifact in request.Manifest.CookedArtifacts) {
            cancellationToken.ThrowIfCancellationRequested();
            string relative = artifact.RelativePath.Replace('\\', '/');
            string source = Path.GetFullPath(Path.Combine(staging, relative));
            if (!source.StartsWith(Path.GetFullPath(staging) + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase)) {
                throw new InvalidOperationException("Cooked artifact escapes the staging directory.");
            }
            string normalized = relative.StartsWith("generated/") ? relative.Substring("generated/".Length) : relative;
            string runtimePath = normalized.StartsWith("cooked/") ? normalized : "cooked/" + normalized;
            string destination = Path.GetFullPath(Path.Combine(packaged, runtimePath));
            if (!destination.StartsWith(Path.GetFullPath(packaged) + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase)) {
                throw new InvalidOperationException("Cooked artifact escapes the package directory.");
            }
            Directory.CreateDirectory(Path.GetDirectoryName(destination));
            File.Copy(source, destination, true);
        }
        await File.WriteAllTextAsync(Path.Combine(packaged, "android-content.json"),
            JsonSerializer.Serialize(request.Manifest, new JsonSerializerOptions { WriteIndented = true }), cancellationToken);
        await File.WriteAllLinesAsync(Path.Combine(packaged, "android-content-files.txt"),
            Directory.GetFiles(Path.Combine(packaged, "cooked"), "*", SearchOption.AllDirectories)
                .Select(path => Path.GetRelativePath(packaged, path).Replace('\\', '/')), cancellationToken);
        progress.Report(new PlatformBuildProgressUpdate("Native APK", "android", 0, 1, "Compiling generated runtime and packaging Vulkan Android player."));
        var start = new ProcessStartInfo("powershell.exe") { UseShellExecute = false };
        string orientation = request.SelectedGraphicsOptionValues.TryGetValue("screen-orientation", out string selectedOrientation)
            ? selectedOrientation : "unspecified";
        string workspaceRoot = Environment.GetEnvironmentVariable("HELENGINE_BUILD_WORKSPACE_ROOT");
        if (string.IsNullOrWhiteSpace(workspaceRoot)) workspaceRoot = request.WorkingRoot;
        foreach (string argument in new[] { "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", Path.Combine(repository, "scripts", "build-host.ps1"),
            "-GeneratedCoreRoot", generated, "-ContentRoot", packaged, "-OutputDirectory", request.OutputRoot,
            "-WorkingDirectory", Path.Combine(request.WorkingRoot, "native-apk"), "-ScreenOrientation", orientation,
            "-NativeBuildRoot", Path.Combine(workspaceRoot, "android-native"), "-GradleCacheRoot", Path.Combine(workspaceRoot, "gradle-cache") }) {
            start.ArgumentList.Add(argument);
        }
        using Process process = Process.Start(start) ?? throw new InvalidOperationException("Android build process could not start.");
        using var registration = cancellationToken.Register(() => process.Kill(true));
        await process.WaitForExitAsync(cancellationToken);
        if (process.ExitCode != 0) throw new InvalidOperationException("Android APK build failed; inspect native-apk/gradle.log.");
        progress.Report(new PlatformBuildProgressUpdate("Native APK", "android", 1, 1, "Verified Android APK built."));
        return new PlatformBuildReport(true, [], request.Manifest.Scenes.Select(scene =>
            new PlatformBuildItemOutcome(scene.SceneId, PlatformBuildItemOutcomeKind.Succeeded)).ToArray(),
            request.Manifest.LooseAssets.Select(asset => new PlatformBuildItemOutcome(asset.AssetId, PlatformBuildItemOutcomeKind.Succeeded)).ToArray());
    }

    /// <summary>Uses a validated explicit checkout for externally published builders, preserving the installed layout otherwise.</summary>
    /// <param name="configuredRoot">Optional HELENGINE_ANDROID_REPOSITORY_ROOT source checkout override.</param>
    /// <param name="assemblyLocation">Installed builder path used only when no override was supplied.</param>
    /// <returns>The full native Android source repository path.</returns>
    internal static string ResolveRepositoryRoot(string configuredRoot, string assemblyLocation) {
        if (!string.IsNullOrWhiteSpace(configuredRoot)) {
            string repository = configuredRoot.Trim();
            if (!Path.IsPathFullyQualified(repository)) throw new InvalidOperationException("HELENGINE_ANDROID_REPOSITORY_ROOT must name an absolute Android source repository.");
            repository = Path.GetFullPath(repository);
            if (!File.Exists(Path.Combine(repository, "CMakeLists.txt")) || !File.Exists(Path.Combine(repository, "scripts", "build-host.ps1"))) {
                throw new InvalidOperationException($"HELENGINE_ANDROID_REPOSITORY_ROOT '{repository}' is not an Android source repository.");
            }
            return repository;
        }
        return Path.GetFullPath(Path.Combine(assemblyLocation, "..", "..", ".."));
    }
}
