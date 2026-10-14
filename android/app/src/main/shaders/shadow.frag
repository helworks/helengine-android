#version 450
layout(set=0, binding=0) uniform sampler2D Texture;
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
layout(location=0) in vec2 VertexTexCoord;
layout(location=1) in float VertexAlpha;
void main() {
    if (Lighting.Options.x > 0.5 && texture(Texture, VertexTexCoord).a * VertexAlpha < 0.5) discard;
}
