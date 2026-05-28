#version 440

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec2 vUV;

layout(std140, binding = 0) uniform buf {
    mat4 mvp;
    float opacity;
    vec3 pad;
} ubuf;

void main()
{
    vUV = inUV;
    gl_Position = ubuf.mvp * vec4(inPos, 0.0, 1.0);
}

