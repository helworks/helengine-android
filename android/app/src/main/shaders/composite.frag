#version 450
layout(set=0,binding=0) uniform sampler2D SceneColor;
layout(location=0) in vec2 TextureCoordinate;
layout(location=0) out vec4 OutputColor;
void main() {
    // The world target contains premultiplied coverage; ONE blending preserves the native-resolution backdrop and UI.
    OutputColor=texture(SceneColor,TextureCoordinate);
}
