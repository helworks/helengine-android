using helengine.baseplatform.Builders;
using helengine.baseplatform.Reporting;

namespace helengine.android.builder.tests;

/// <summary>Records actual build progress so a failed native process cannot report completion.</summary>
public sealed class AndroidHostToolsProgressReporter : IPlatformBuildProgressReporter {
    /// <summary>Build progress emitted before the host script fails.</summary>
    public List<PlatformBuildProgressUpdate> Updates { get; } = [];

    /// <summary>Records the supplied update.</summary>
    public void Report(PlatformBuildProgressUpdate update) => Updates.Add(update);
}
