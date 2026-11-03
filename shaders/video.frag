#version 440

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D texSampler;

layout(std140, binding = 0) uniform buf {
    mat4 mvp;
    float opacity;
    float black;
} ubuf;

void main()
{
    // 视频与桌面捕获按不透明图像处理，合成时应用 Qt 继承透明度。
    fragColor = vec4(mix(texture(texSampler, vUV).rgb, vec3(0.0), ubuf.black) * ubuf.opacity, ubuf.opacity);
}



