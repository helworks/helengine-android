#version 450
layout(location=1) in vec4 Color;
layout(location=2) in vec2 TexCoord;
layout(location=3) in vec3 WorldPosition;
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
} Lighting;
layout(location=0) out vec2 VertexTexCoord;
layout(location=1) out float VertexAlpha;
void main() {
    gl_Position = Lighting.ShadowMatrix * vec4(WorldPosition, 1.0);
    VertexTexCoord = TexCoord;
    VertexAlpha = Color.a;
}
