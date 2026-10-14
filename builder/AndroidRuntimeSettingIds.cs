namespace helengine.android.builder;

/// <summary>Names Android runtime contracts exposed in project presets and the engine's settings API.</summary>
public static class AndroidRuntimeSettingIds {
    /// <summary>Scales the 3D color and depth targets while retaining native-resolution overlays and input coordinates.</summary>
    public const string RenderScale = "render-scale";
    /// <summary>Requests an explicit presentation rate in FPS; zero follows the available display refresh rate.</summary>
    public const string TargetFrameRate = "target-frame-rate";
}
