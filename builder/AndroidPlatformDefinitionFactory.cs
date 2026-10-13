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
                    "debug", "Debug APK", "Native Vulkan player packaged with debug signing.",
                    "android-native", "default", [])
            ],
            [
                new PlatformGraphicsProfileDefinition(
                    "android-native", "Vulkan",
                    "Native scene rendering with untextured directional-lit meshes and 2D overlays.", [
                        new PlatformSettingDefinition("screen-orientation", "Screen Orientation", PlatformSettingKind.Choice,
                            "unspecified", true, ["unspecified", "portrait", "landscape"])
                    ])
            ],
            [new PlatformAssetRequirementDefinition("scene", "Scene", true, ["helen"])],
            [new PlatformMaterialSchemaDefinition("standard-shader", "Standard Material", ["android-native"], [
                new PlatformMaterialFieldDefinition("base-color", "Base Color", PlatformMaterialFieldKind.Color, "#ffffff", false, []),
                new PlatformMaterialFieldDefinition("texture-id", "Texture", PlatformMaterialFieldKind.AssetReference, "", false, []),
                new PlatformMaterialFieldDefinition("shader-asset-id", "Shader", PlatformMaterialFieldKind.AssetReference, "", false, []),
                new PlatformMaterialFieldDefinition("vertex-program", "Vertex Program", PlatformMaterialFieldKind.Text, "", false, []),
                new PlatformMaterialFieldDefinition("pixel-program", "Pixel Program", PlatformMaterialFieldKind.Text, "", false, []),
                new PlatformMaterialFieldDefinition("variant", "Variant", PlatformMaterialFieldKind.Text, "", false, [])
            ])],
            [],
            [
                new PlatformCodegenProfileDefinition(
                    "default", "Native C++", "Generated Android 64-bit engine and gameplay runtime.",
                    PlatformCodegenLanguage.Cpp,
                    PlatformSerializationEndianness.LittleEndian,
                    [
                        new PlatformSettingDefinition(
                            "pointer-size-bytes", "Pointer Size (Bytes)", PlatformSettingKind.Text,
                            "8", true, []),
                        new PlatformSettingDefinition(
                            "codegen-use-rtti", "Use Compiler RTTI", PlatformSettingKind.Boolean,
                            "true", true, []),
                        new PlatformSettingDefinition("generated-math-convention", "Math Convention", PlatformSettingKind.Text,
                            "native-column-vector", true, []),
                        new PlatformSettingDefinition("type-remaps", "Numeric Types", PlatformSettingKind.Text,
                            "System.Numerics.Vector2=helengine.float2;System.Numerics.Vector3=helengine.float3;System.Numerics.Vector4=helengine.float4;System.Numerics.Quaternion=helengine.float4", true, []),
                        new PlatformSettingDefinition(PlatformCodegenSettingIds.EnabledFeatures, "Runtime Features", PlatformSettingKind.Text,
                            "host_file_system", true, []),
                        new PlatformSettingDefinition(
                            "codegen-use-exceptions", "Use C++ Exceptions", PlatformSettingKind.Boolean,
                            "true", true, [])
                    ])
            ],
            [new PlatformStorageProfileDefinition("apk-assets", "APK Assets", PlatformStorageProfileKind.LooseFiles, "android-vulkan-apk", false)],
            [new PlatformMediaProfileDefinition("android-apk", "Android APK", PlatformMediaLayoutKind.InstallTree, false, true)],
            new RuntimeGenerationContract(RuntimeMaterialResolutionMode.CookedPlatformOwned, true, PackagedPathPolicy.ContentRelativeOnly));
    }
}
