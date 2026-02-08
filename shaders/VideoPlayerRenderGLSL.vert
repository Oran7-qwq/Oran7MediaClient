#version 440

layout(location = 0) in vec4 qt_Vertex;
layout(location = 1) in vec2 qt_MultiTexCoord0;

layout(location = 0) out vec2 texCoord;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 textureCoord;
    vec2 textureOffset;
} ubuf;

out gl_PerVertex { vec4 gl_Position; };

void main() {
    gl_Position = ubuf.qt_Matrix * qt_Vertex;
    texCoord = qt_MultiTexCoord0;
}
