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
        string repository = Path.GetFullPath(Path.Combine(typeof(AndroidBuildWorkspace).Assembly.Location, "..", "..", ".."));
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
        foreach (string argument in new[] { "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", Path.Combine(repository, "scripts", "build-host.ps1"),
            "-GeneratedCoreRoot", generated, "-ContentRoot", packaged, "-OutputDirectory", request.OutputRoot,
            "-WorkingDirectory", Path.Combine(request.WorkingRoot, "native-apk"), "-ScreenOrientation", orientation }) {
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
}
