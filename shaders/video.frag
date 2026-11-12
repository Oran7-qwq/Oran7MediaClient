#version 440

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D texSampler;
layout(binding = 2) uniform sampler2D chromaSampler;

layout(std140, binding = 0) uniform buf {
    mat4 mvp;
    float opacity;
    float black;
    vec2 padding;
    vec4 mode;
    vec4 range;
    vec4 coefficients;
    vec4 uvScale;
} ubuf;

// 扩展 sRGB 允许 FP16 中保留 >1 和负数，最终窗口合成再还原为线性 scRGB。
vec3 srgbEncode(vec3 x)
{
    vec3 a = abs(x);
    return sign(x) * mix(12.92 * a, 1.055 * pow(a, vec3(1.0 / 2.4)) - 0.055,
                        step(vec3(0.0031308), a));
}
vec3 toLinear(vec3 encoded)
{
    vec3 rgb = max(encoded, vec3(0));
    if (ubuf.mode.y < 1.5) {
        // ST.2084 的绝对亮度（nit），不能当作普通 gamma 值直接显示。
        vec3 p = pow(rgb, vec3(1.0 / 78.84375));
        return 10000.0 * pow(max(p - 0.8359375, vec3(0)) /
                            max(18.8515625 - 18.6875 * p, vec3(1e-6)), vec3(1.0 / 0.1593017578125));
    }
    // HLG inverse OETF + 1000 nit 参考显示的 OOTF（系统 gamma 1.2）。
    vec3 scene = mix(rgb * rgb / 3.0,
                     (exp((rgb - 0.55991073) / 0.17883277) + 0.28466892) / 12.0,
                     step(vec3(0.5), rgb));
    float y = max(dot(scene, mix(vec3(0.2126, 0.7152, 0.0722), vec3(0.2627, 0.6780, 0.0593), ubuf.mode.z)), 1e-6);
    return scene * pow(y, 0.2) * 1000.0;
}
vec3 gamut709(vec3 rgb)
{
    if (ubuf.mode.z < 0.5) return rgb;
    return vec3(dot(rgb, vec3(1.660491, -0.587641, -0.072850)),
                dot(rgb, vec3(-0.124550, 1.132900, -0.008349)),
                dot(rgb, vec3(-0.018151, -0.100579, 1.118730)));
}
vec3 toneMap(vec3 nits)
{
    // 亮度驱动的扩展 Reinhard，保留通道比例；只对超出 SDR 色域的颜色压缩饱和度。
    float luminance = max(dot(nits, vec3(0.2126, 0.7152, 0.0722)), 0.0);
    float x = luminance / 203.0;
    float white = max(ubuf.coefficients.z / 203.0, 1.0);
    float y = clamp(x * (1.0 + x / (white * white)) / (1.0 + x), 0.0, 1.0);
    vec3 color = nits * (y / max(luminance, 1e-6));
    vec3 delta = color - vec3(y);
    vec3 upper = vec3(1.0 - y) / max(delta, vec3(1e-6));
    vec3 lower = vec3(y) / max(-delta, vec3(1e-6));
    vec3 bounds = mix(upper, lower, lessThan(delta, vec3(0)));
    float saturation = clamp(min(bounds.r, min(bounds.g, bounds.b)), 0.0, 1.0);
    return clamp(vec3(y) + saturation * delta, 0.0, 1.0);
}
void main()
{
    vec2 uv = vUV * ubuf.uvScale.xy;
    vec3 rgb = texture(texSampler, uv).rgb;
    if (ubuf.mode.x > 0.5) {
        float y = (rgb.r - ubuf.range.x) * ubuf.range.y;
        vec2 cbcr = (texture(chromaSampler, uv).rg - ubuf.range.z) * ubuf.range.w;
        float kr = ubuf.coefficients.x, kb = ubuf.coefficients.y, kg = 1.0 - kr - kb;
        rgb = vec3(y + 2.0 * (1.0 - kr) * cbcr.y,
                   y - 2.0 * kb * (1.0 - kb) / kg * cbcr.x - 2.0 * kr * (1.0 - kr) / kg * cbcr.y,
                   y + 2.0 * (1.0 - kb) * cbcr.x);
    }
    bool linearOutput = false;
    if (ubuf.mode.y > 0.5) {
        vec3 nits = gamut709(toLinear(rgb));
        if (ubuf.mode.w > 0.5) rgb = nits / ubuf.coefficients.w;
        else rgb = toneMap(nits);
        linearOutput = true;
    } else if (ubuf.mode.z > 0.5) {
        vec3 linear = mix(rgb / 4.5, pow(max((rgb + 0.099) / 1.099, vec3(0)), vec3(1.0 / 0.45)),
                          step(vec3(0.081), rgb));
        rgb = clamp(gamut709(linear), 0.0, 1.0);
        linearOutput = true;
    }
    if (linearOutput) rgb = srgbEncode(rgb);
    fragColor = vec4(mix(rgb, vec3(0.0), ubuf.black) * ubuf.opacity, ubuf.opacity);
}
