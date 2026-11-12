#version 440
layout(location = 0) out vec2 vUV;
void main()
{
    vUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(vUV * vec2(2, -2) + vec2(-1, 1), 0, 1);
}
