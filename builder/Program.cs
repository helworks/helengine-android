using System.Text.Json;
using helengine.android.builder;

/// <summary>
/// Exposes Android platform metadata for command-line inspection without an Android toolchain.
/// </summary>
public static class Program {
    /// <summary>
    /// Prints the same metadata the editor receives from the Android platform plugin.
    /// </summary>
    /// <param name="args">The supported --describe switch.</param>
    /// <returns>Zero for a metadata query; two for unsupported arguments.</returns>
    public static int Main(string[] args) {
        if (args.Length == 1 && args[0] == "--describe") {
            AndroidPlatformAssetBuilder builder = new();
            Console.WriteLine(JsonSerializer.Serialize(builder.Definition, new JsonSerializerOptions {
                WriteIndented = true
            }));
            return 0;
        }

        Console.Error.WriteLine("Usage: helengine.android.builder --describe");
        return 2;
    }
}
