using System.Diagnostics;
using System.Runtime.CompilerServices;
using System.Text;

namespace helengine.android.builder.tests;

/// <summary>Provides disposable pinned-layout tool markers and a real executable Java failure fixture.</summary>
public sealed class AndroidHostToolsFixture : IDisposable {
    private readonly Dictionary<string, string> savedEnvironment = [];
    /// <summary>The same fixed temporary test directory is reused on every invocation.</summary>
    public string Root { get; } = Path.Combine(OperatingSystem.IsWindows() ? @"C:\temp\helengine\android" : "/private/tmp/helengine/android", "host-tool-tests");
    /// <summary>The actual source checkout, never an exported copy of the script.</summary>
    public string Repository { get; }
    public string JavaRoot => Path.Combine(Root, "jdk 17");
    public string SdkRoot => Path.Combine(Root, "android sdk");
    public string NativeRoot => Path.Combine(Root, "native build");
    public string WorkingRoot => Path.Combine(Root, "working");
    public string OutputRoot => Path.Combine(Root, "output");
    public string TracePath => Path.Combine(Root, "java-arguments.txt");
    public string[] RequiredTools { get; }

    /// <summary>Creates only marker files; no SDK, Gradle distribution, or Java installation is downloaded.</summary>
    public AndroidHostToolsFixture([CallerFilePath] string sourcePath = "") {
        Repository = Path.GetFullPath(Path.Combine(Path.GetDirectoryName(sourcePath)!, ".."));
        if (Directory.Exists(Root)) Directory.Delete(Root, true);
        Directory.CreateDirectory(JavaRoot);
        File.WriteAllText(Path.Combine(JavaRoot, "release"), "JAVA_VERSION=\"17.0.1\"");
        string suffix = OperatingSystem.IsWindows() ? ".exe" : "";
        string tag = OperatingSystem.IsWindows() ? "windows-x86_64" : OperatingSystem.IsMacOS() ? "darwin-x86_64" : "linux-x86_64";
        RequiredTools = [
            Path.Combine(SdkRoot, "platforms", "android-35", "android.jar"),
            Path.Combine(SdkRoot, "cmake", "3.22.1", "bin", "cmake" + suffix),
            Path.Combine(SdkRoot, "build-tools", "35.0.0", "apksigner" + (OperatingSystem.IsWindows() ? ".bat" : "")),
            Path.Combine(SdkRoot, "build-tools", "35.0.0", "aapt" + suffix),
            Path.Combine(SdkRoot, "build-tools", "35.0.0", "zipalign" + suffix),
            Path.Combine(SdkRoot, "ndk", "27.0.12077973", "toolchains", "llvm", "prebuilt", tag, "bin", "llvm-readelf" + suffix)
        ];
        foreach (string path in RequiredTools) {
            Directory.CreateDirectory(Path.GetDirectoryName(path)!);
            File.WriteAllText(path, "fixture marker; never executed");
        }
        string java = Path.Combine(JavaRoot, "bin", "java" + suffix);
        Directory.CreateDirectory(Path.GetDirectoryName(java)!);
        if (OperatingSystem.IsWindows()) {
            string source = Path.Combine(Root, "JavaFixture.cs");
            File.WriteAllText(source, "using System; using System.IO; class JavaFixture { static int Main(string[] args) { File.WriteAllLines(Environment.GetEnvironmentVariable(\"HELENGINE_ANDROID_HOST_TEST_TRACE\"), args); Console.Error.WriteLine(\"fixture-java-exit-37\"); return 37; } }");
            var compiler = new ProcessStartInfo(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows), "Microsoft.NET", "Framework64", "v4.0.30319", "csc.exe")) { UseShellExecute = false };
            foreach (string argument in new[] { "/nologo", "/target:exe", "/out:" + java, source }) compiler.ArgumentList.Add(argument);
            using var child = Process.Start(compiler)!;
            child.WaitForExit();
            if (child.ExitCode != 0) throw new InvalidOperationException("Could not compile the isolated Java failure fixture.");
        } else {
            File.WriteAllText(java, "#!/bin/sh\nprintf '%s\\n' \"$@\" > \"$HELENGINE_ANDROID_HOST_TEST_TRACE\"\nprintf '%s\\n' fixture-java-exit-37 >&2\nexit 37\n");
            File.SetUnixFileMode(java, UnixFileMode.UserRead | UnixFileMode.UserWrite | UnixFileMode.UserExecute);
        }
    }

    /// <summary>Invokes the production script using this host's installed PowerShell.</summary>
    public async Task<(int ExitCode, string Output)> RunScript() {
        var start = new ProcessStartInfo(OperatingSystem.IsWindows() ? "powershell.exe" : "pwsh") {
            UseShellExecute = false, RedirectStandardOutput = true, RedirectStandardError = true
        };
        string[] scriptArguments = [Path.Combine(Repository, "scripts", "build-host.ps1"),
            "-SdkRoot", SdkRoot, "-JavaHome", JavaRoot, "-OutputDirectory", OutputRoot, "-WorkingDirectory", WorkingRoot,
            "-GradleCacheRoot", Path.Combine(Root, "gradle cache"), "-NativeBuildRoot", NativeRoot];
        // Print the actual exception message without the host's terminal-width-dependent error rendering.
        string command = "try { & " + string.Join(" ", scriptArguments.Select((argument, index) => index % 2 == 1
            ? argument : "'" + argument.Replace("'", "''") + "'")) +
            " } catch { [Console]::Error.WriteLine($_.Exception.Message); exit 1 }";
        foreach (string argument in new[] { "-NoProfile", "-ExecutionPolicy", "Bypass", "-EncodedCommand", Convert.ToBase64String(Encoding.Unicode.GetBytes(command)) })
            start.ArgumentList.Add(argument);
        start.Environment["HELENGINE_ANDROID_HOST_TEST_TRACE"] = TracePath;
        start.Environment["TERM"] = "dumb";
        start.Environment["NO_COLOR"] = "1";
        using var child = Process.Start(start)!;
        Task<string> stdout = child.StandardOutput.ReadToEndAsync();
        Task<string> stderr = child.StandardError.ReadToEndAsync();
        await child.WaitForExitAsync();
        return (child.ExitCode, await stdout + await stderr);
    }

    /// <summary>Configures the public builder entry point and restores every variable when disposed.</summary>
    public void SetWorkspaceEnvironment() {
        foreach (var pair in new Dictionary<string, string> {
            ["HELENGINE_ANDROID_REPOSITORY_ROOT"] = Repository, ["HELENGINE_BUILD_WORKSPACE_ROOT"] = Root,
            ["JAVA_HOME"] = JavaRoot, ["ANDROID_HOME"] = SdkRoot, ["ANDROID_SDK_ROOT"] = SdkRoot,
            ["HELENGINE_ANDROID_HOST_TEST_TRACE"] = TracePath
        }) {
            savedEnvironment[pair.Key] = Environment.GetEnvironmentVariable(pair.Key);
            Environment.SetEnvironmentVariable(pair.Key, pair.Value);
        }
    }

    /// <summary>Restores process configuration and removes the isolated tool markers.</summary>
    public void Dispose() {
        foreach (var pair in savedEnvironment) Environment.SetEnvironmentVariable(pair.Key, pair.Value);
        if (Directory.Exists(Root)) Directory.Delete(Root, true);
    }
}
