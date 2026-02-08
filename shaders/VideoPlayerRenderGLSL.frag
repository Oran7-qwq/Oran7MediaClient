#version 440

layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 textureCoord;
    vec2 textureOffset;
} ubuf;

layout(binding = 1) uniform sampler2D sourceTexture;

void main() {
    vec4 color = texture(sourceTexture, texCoord);
    fragColor = color * ubuf.qt_Opacity;
}
