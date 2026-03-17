// #version 440

// layout(location = 0) in vec2 vUV;
// layout(location = 0) out vec4 fragColor;

// layout(std140, binding = 0) uniform buf {
//     mat4 mvp;
//     float opacity;
//     vec3 pad;
// } ubuf;

// layout(binding = 1) uniform sampler2D texSampler;

// void main()
// {
//     vec4 c = texture(texSampler, vUV);
//     c.a *= ubuf.opacity;
//     fragColor = c;
// }

#version 440

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D texSampler;

void main()
{
    fragColor = vec4(texture(texSampler, vUV).rgb, 1.0);
}



