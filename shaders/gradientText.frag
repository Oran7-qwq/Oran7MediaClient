#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D source;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 beginColor;
    vec4 middleColor;
    vec4 endColor;
} ubuf;

void main()
{
    // 文字 layer 的 alpha（笔画形状）
    float alpha = texture(source, qt_TexCoord0).a;

    // 水平三段渐变：begin → middle → end（保留原 colorItems 的三段配色）
    float t = qt_TexCoord0.x;
    vec3 col;
    if (t < 0.5)
        col = mix(ubuf.beginColor.rgb, ubuf.middleColor.rgb, t * 2.0);
    else
        col = mix(ubuf.middleColor.rgb, ubuf.endColor.rgb, (t - 0.5) * 2.0);

    // 直接用渐变替换文字颜色，只保留 alpha → 无白色垫底层，无描边阴影
    fragColor = vec4(col, 1.0) * alpha * ubuf.qt_Opacity;
}
