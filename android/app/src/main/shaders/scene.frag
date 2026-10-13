#version 450
layout(set=0, binding=0) uniform sampler2D Texture;
layout(location=0) in vec4 VertexColor;
layout(location=1) in vec2 VertexTexCoord;
layout(location=0) out vec4 OutputColor;
void main() { OutputColor = texture(Texture, VertexTexCoord) * VertexColor; }
