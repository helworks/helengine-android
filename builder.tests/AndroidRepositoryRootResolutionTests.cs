using helengine.android.builder;

namespace helengine.android.builder.tests;

/// <summary>Verifies source checkout resolution when the builder is deployed outside its repository.</summary>
public sealed class AndroidRepositoryRootResolutionTests {
    /// <summary>An explicit real checkout takes precedence over an unrelated artifacts directory.</summary>
    [Fact]
    public void ExplicitCheckoutOverridesAssemblyLocation() {
        string repository = Path.Combine(Path.GetTempPath(), "repository-resolution-tests");
        Directory.CreateDirectory(Path.Combine(repository, "scripts"));
        File.WriteAllText(Path.Combine(repository, "CMakeLists.txt"), "test marker");
        File.WriteAllText(Path.Combine(repository, "scripts", "build-host.ps1"), "test marker");
        Assert.Equal(Path.GetFullPath(repository), AndroidBuildWorkspace.ResolveRepositoryRoot(repository, Path.Combine(repository, "unrelated", "builder.dll")));
    }

    /// <summary>A relative override is rejected instead of silently resolving against the caller's directory.</summary>
    [Fact]
    public void RelativeCheckoutIsRejected() {
        Assert.Throws<InvalidOperationException>(() => AndroidBuildWorkspace.ResolveRepositoryRoot("helengine-android", "ignored.dll"));
    }

    /// <summary>A fully qualified nonrepository override cannot silently fall back to the installed assembly.</summary>
    [Fact]
    public void NonRepositoryOverrideIsRejected() {
        Assert.Throws<InvalidOperationException>(() => AndroidBuildWorkspace.ResolveRepositoryRoot(Path.GetPathRoot(AppContext.BaseDirectory), "ignored.dll"));
    }

    /// <summary>Installed builders retain their established artifacts/builder source lookup without an override.</summary>
    [Fact]
    public void InstalledLayoutRetainsRepositoryLookup() {
        string repository = Path.Combine(Path.GetTempPath(), "installed-repository");
        Assert.Equal(repository, AndroidBuildWorkspace.ResolveRepositoryRoot(string.Empty, Path.Combine(repository, "artifacts", "builder", "builder.dll")));
    }
}
