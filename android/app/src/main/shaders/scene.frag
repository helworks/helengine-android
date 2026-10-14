#version 450
layout(set=0, binding=0) uniform sampler2D Texture;
layout(set=1, binding=0) uniform sampler2D RoughnessTexture;
layout(set=3, binding=0) uniform sampler2DShadow ShadowTexture;
layout(std140, set=2, binding=0) uniform Shading {
    vec4 Camera;
    vec4 Ambient;
    vec4 Surface;
    vec4 Options;
    vec4 LightPositions[8];
    vec4 LightColors[8];
    vec4 LightDirections[8];
    vec4 LightParameters[8];
    mat4 ShadowMatrix;
    vec4 ShadowParameters;
    vec4 RenderOptions;
} Lighting;
layout(location=0) in vec4 VertexColor;
layout(location=1) in vec2 VertexTexCoord;
layout(location=2) in vec3 VertexWorldPosition;
layout(location=3) in vec3 VertexNormal;
layout(location=0) out vec4 OutputColor;
const float Pi = 3.141592653589793;

float DirectionalVisibility() {
    if (Lighting.ShadowParameters.x < 0.5 || Lighting.ShadowParameters.z < 0.5) return 1.0;
    if (distance(VertexWorldPosition, Lighting.Camera.xyz) > Lighting.ShadowParameters.w) return 1.0;
    vec3 projected = (Lighting.ShadowMatrix * vec4(VertexWorldPosition, 1.0)).xyz;
    vec2 uv = projected.xy * 0.5 + 0.5;
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 || projected.z < 0.0 || projected.z > 1.0) return 1.0;
    float visibility = 0.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            visibility += texture(ShadowTexture, vec3(uv + vec2(x, y) / 1024.0, projected.z - Lighting.ShadowParameters.y));
        }
    }
    return visibility / 9.0;
}

// Cook-Torrance GGX shading preserves authored metallic and roughness independently of camera or vertex density.
vec3 Shade(vec3 albedo) {
    vec3 normal = normalize(VertexNormal);
    vec3 viewDirection = normalize(Lighting.Camera.xyz - VertexWorldPosition);
    float metallic = Lighting.Surface.x;
    float roughness = Lighting.Surface.y;
    if (Lighting.Options.y > 0.5) roughness *= texture(RoughnessTexture, VertexTexCoord).r;
    roughness = clamp(roughness, 0.045, 1.0);
    float alpha = roughness * roughness;
    float alphaSquared = alpha * alpha;
    float geometryK = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    float nDotV = max(dot(normal, viewDirection), 0.0001);
    vec3 f0 = mix(vec3(0.08 * Lighting.Surface.z), albedo, metallic);
    vec3 color = Lighting.Ambient.rgb * albedo;
    for (int index = 0; index < int(Lighting.Options.z); ++index) {
        vec3 lightDirection;
        float attenuation = 1.0;
        if (Lighting.LightPositions[index].w < 0.5) {
            lightDirection = normalize(-Lighting.LightPositions[index].xyz);
            if (index == 0) attenuation = DirectionalVisibility();
        } else {
            vec3 delta = Lighting.LightPositions[index].xyz - VertexWorldPosition;
            float distanceToLight = length(delta);
            lightDirection = delta / max(distanceToLight, 0.0001);
            float range = Lighting.LightParameters[index].x;
            attenuation = pow(clamp(1.0 - distanceToLight / max(range, 0.0001), 0.0, 1.0), 2.0);
            if (Lighting.LightPositions[index].w > 1.5) {
                float cone = dot(-lightDirection, Lighting.LightDirections[index].xyz);
                attenuation *= smoothstep(Lighting.LightParameters[index].z, Lighting.LightParameters[index].y, cone);
            }
        }
        float nDotL = max(dot(normal, lightDirection), 0.0);
        vec3 halfway = normalize(viewDirection + lightDirection);
        float nDotH = max(dot(normal, halfway), 0.0);
        float hDotV = max(dot(halfway, viewDirection), 0.0);
        float distributionDenominator = nDotH * nDotH * (alphaSquared - 1.0) + 1.0;
        float distribution = alphaSquared / max(Pi * distributionDenominator * distributionDenominator, 0.000001);
        float geometry = nDotV / (nDotV * (1.0 - geometryK) + geometryK);
        geometry *= nDotL / max(nDotL * (1.0 - geometryK) + geometryK, 0.0001);
        vec3 fresnel = f0 + (1.0 - f0) * pow(1.0 - hDotV, 5.0);
        vec3 specular = distribution * geometry * fresnel / max(4.0 * nDotV * nDotL, 0.0001);
        vec3 diffuse = (1.0 - fresnel) * (1.0 - metallic) * albedo / Pi;
        color += (diffuse + specular) * Lighting.LightColors[index].rgb * nDotL * attenuation;
    }
    return color;
}

void main() {
    vec4 sampled = texture(Texture, VertexTexCoord);
    float alpha = sampled.a * VertexColor.a;
    if (Lighting.Options.x > 0.5 && Lighting.Options.x < 1.5 && alpha < 0.5) discard;
    if (Lighting.RenderOptions.x > 0.5 && Lighting.Options.x < 1.5) alpha = 1.0;
    if (Lighting.Surface.w < 0.5) {
        vec3 color = sampled.rgb * VertexColor.rgb;
        if (Lighting.Options.w > 0.5) color = pow(color, vec3(2.2));
        OutputColor = vec4(color, alpha);
        return;
    }
    vec3 albedo = sampled.rgb * pow(VertexColor.rgb, vec3(2.2));
    vec3 color = Shade(albedo);
    color = color / (color + vec3(1.0));
    if (Lighting.Options.w < 0.5) color = pow(max(color, vec3(0.0)), vec3(1.0 / 2.2));
    OutputColor = vec4(color, alpha);
}
