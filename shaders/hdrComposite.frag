#version 440
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 fragColor;
layout(binding = 0) uniform sampler2D source;
layout(std140, binding = 1) uniform Output { vec4 white; } outputInfo;
void main()
{
    vec4 pixel = texture(source, vUV);
    vec3 encoded = pixel.a > 1e-6 ? pixel.rgb / pixel.a : vec3(0);
    vec3 a = abs(encoded);
    vec3 linear = sign(encoded) * mix(a / 12.92, pow((a + 0.055) / 1.055, vec3(2.4)),
                                     step(vec3(0.04045), a));
    // scRGB 1.0 = 80 nit；界面白色跟随 Windows SDR 内容亮度，视频仍保持绝对 nit。
    fragColor = vec4(linear * pixel.a * outputInfo.white.x, pixel.a);
}
