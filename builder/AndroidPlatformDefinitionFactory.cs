using helengine.baseplatform.Definitions;
using helengine.baseplatform.Profiles;

namespace helengine.android.builder;

/// <summary>
/// Provides Android editor metadata while the native runtime and game exporter are being developed.
/// </summary>
public static class AndroidPlatformDefinitionFactory {
    /// <summary>
    /// Creates selectable build profiles and the initial 64-bit native-host contract.
    /// </summary>
    /// <returns>Android platform metadata consumed by the editor's existing plugin loader.</returns>
    public static PlatformDefinition Create() {
        return new PlatformDefinition(
            "android",
            "Android",
            [
                new PlatformBuildProfileDefinition(
                    "debug", "Debug", "Android scaffold; game export is pending.",
                    "android-native", "default", []),
                new PlatformBuildProfileDefinition(
                    "release", "Release", "Android scaffold; game export and signing are pending.",
                    "android-native", "default", [])
            ],
            [
                new PlatformGraphicsProfileDefinition(
                    "android-native", "Native Host (Scaffold)",
                    "NativeActivity lifecycle scaffold. Rendering is not implemented.", [])
            ],
            [],
            [],
            [],
            [
                new PlatformCodegenProfileDefinition(
                    "default", "Native C++", "Reserved for the Android 64-bit runtime integration.",
                    PlatformCodegenLanguage.Cpp,
                    PlatformSerializationEndianness.LittleEndian,
                    [
                        new PlatformSettingDefinition(
                            "pointer-size-bytes", "Pointer Size (Bytes)", PlatformSettingKind.Text,
                            "8", true, []),
                        new PlatformSettingDefinition(
                            "codegen-use-rtti", "Use Compiler RTTI", PlatformSettingKind.Boolean,
                            "true", true, []),
                        new PlatformSettingDefinition(
                            "codegen-use-exceptions", "Use C++ Exceptions", PlatformSettingKind.Boolean,
                            "true", true, [])
                    ])
            ]);
    }
}
