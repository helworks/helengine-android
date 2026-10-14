using System.Globalization;
using helengine;

namespace helengine.android.builder;

/// <summary>Appends a versioned Android shading payload after the shared material identity and texture dependency record.</summary>
public static class AndroidMaterialParametersSerializer {
    /// <summary>Identifies the Android material parameter extension independently of the shared HELE asset version.</summary>
    public const uint Magic = 0x414d4548;
    /// <summary>Version of metallic, roughness, specular, shadow, alpha and roughness-texture parameters.</summary>
    public const byte Version = 1;

    /// <summary>Writes the common dependency-readable material followed by the native Vulkan shading parameters.</summary>
    /// <param name="material">Common cooked material record consumed by the engine and dependency packager.</param>
    /// <param name="fields">Resolved authored schema values for Android.</param>
    /// <returns>A complete Android cooked material file.</returns>
    public static byte[] Serialize(PlatformMaterialAsset material, IReadOnlyDictionary<string, string> fields) {
        byte[] common = global::helengine.files.AssetSerializer.SerializeToBytes(material);
        using MemoryStream stream = new();
        stream.Write(common);
        using EngineBinaryWriter writer = EngineBinaryWriter.Create(stream, EngineBinaryEndianness.LittleEndian);
        writer.WriteUInt32(Magic);
        writer.WriteByte(Version);
        writer.WriteSingle(ReadUnitScalar(fields, "metallic", 0f));
        writer.WriteSingle(ReadUnitScalar(fields, "roughness", 0.5f));
        writer.WriteSingle(ReadUnitScalar(fields, "specular", 0.5f));
        writer.WriteByte(ReadBoolean(fields, "casts-shadows", true) ? (byte)1 : (byte)0);
        writer.WriteByte(ReadBoolean(fields, "receives-shadows", true) ? (byte)1 : (byte)0);
        string alpha = fields.TryGetValue("alpha-mode", out string mode) ? mode : "opaque";
        writer.WriteByte(alpha switch { "opaque" => (byte)0, "alpha-test" => (byte)1, "alpha-blend" => (byte)2,
            _ => throw new NotSupportedException($"Unsupported Android material alpha mode '{alpha}'.") });
        writer.WriteString(ResolveRoughnessTexture(fields));
        return stream.ToArray();
    }

    /// <summary>Reads finite unit-range material coefficients using the authored invariant decimal convention.</summary>
    /// <param name="fields">Resolved material field values.</param>
    /// <param name="name">Coefficient field id.</param>
    /// <param name="defaultValue">Standard-material value used when the coefficient is unauthored.</param>
    /// <returns>The validated scalar.</returns>
    static float ReadUnitScalar(IReadOnlyDictionary<string, string> fields, string name, float defaultValue) {
        if (!fields.TryGetValue(name, out string text) || string.IsNullOrWhiteSpace(text)) {
            return defaultValue;
        }
        if (!float.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out float value) || !float.IsFinite(value) || value < 0f || value > 1f) {
            throw new InvalidOperationException($"Android material '{name}' must be a finite value between zero and one.");
        }
        return value;
    }

    /// <summary>Preserves authored shadow policy and rejects malformed boolean fields.</summary>
    /// <param name="fields">Resolved material field values.</param>
    /// <param name="name">Boolean field id.</param>
    /// <param name="defaultValue">Standard policy used when no override exists.</param>
    /// <returns>The authored policy.</returns>
    static bool ReadBoolean(IReadOnlyDictionary<string, string> fields, string name, bool defaultValue) {
        if (!fields.TryGetValue(name, out string text) || string.IsNullOrWhiteSpace(text)) {
            return defaultValue;
        }
        if (!bool.TryParse(text, out bool value)) {
            throw new InvalidOperationException($"Android material '{name}' must be a boolean.");
        }
        return value;
    }

    /// <summary>Resolves a roughness dependency using Android's published default imported-texture naming contract.</summary>
    /// <param name="fields">Cook request with imported texture identities already projected from asset references.</param>
    /// <returns>The content-relative roughness texture path, or an empty string for scalar roughness.</returns>
    static string ResolveRoughnessTexture(IReadOnlyDictionary<string, string> fields) {
        if (!fields.TryGetValue("roughness-texture-id", out string id) || string.IsNullOrWhiteSpace(id)) {
            return string.Empty;
        }
        if (id.Contains('/') || id.Contains('\\') || id.Contains(':') || id == "." || id == "..") {
            throw new InvalidOperationException("Android roughness textures require resolved imported asset identities.");
        }
        return "cooked/imported/" + id;
    }
}
