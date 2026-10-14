#version 450
layout(location=0) in vec4 Position;
layout(location=1) in vec4 Color;
layout(location=2) in vec2 TexCoord;
layout(location=3) in vec3 WorldPosition;
layout(location=4) in vec3 Normal;
layout(location=0) out vec4 VertexColor;
layout(location=1) out vec2 VertexTexCoord;
layout(location=2) out vec3 VertexWorldPosition;
layout(location=3) out vec3 VertexNormal;
void main() {
    gl_Position = Position;
    VertexColor = Color;
    VertexTexCoord = TexCoord;
    VertexWorldPosition = WorldPosition;
    VertexNormal = Normal;
}
