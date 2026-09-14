#version 440

// ============================================================================
//  Oran7 歌词文字特效 —— clouds (Shadertoy，fbm 域扭曲)
//
//  【契约头】本文件 "契约头结束" 之前的内容，每个 shader 文件原样保留。
//  QML 侧固定提供 iResolution / iTime 两个 uniform，不要改名或改类型，
//  这样换 shader 只需要替换本文件、不需要动任何 QML。
// ============================================================================
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(binding = 1) uniform sampler2D source;      // 文字层纹理，由 layer 自动绑定

layout(std140, binding = 0) uniform buf {
    mat4  qt_Matrix;      // ← 前两个成员顺序不可动（Qt 约定）
    float qt_Opacity;
    vec3  iResolution;    // 组件真实像素尺寸，由 QML 传入
    float iTime;          // 秒
} ubuf;
// ========================= 契约头结束 =======================================

// ---------------------------------------------------------------------------
//  取景：SCENE_ASPECT 这一份必须取 1.0
//
//  这份 shader 的坐标是 `uv = fragCoord / iResolution.xy` —— **两个轴各自**
//  归一化到 0..1。代进包装层会发现 virtRes 被完全约掉：
//        fragCoord = centered * virtRes
//        uv        = fragCoord / iResolution.xy = centered
//  所以虚拟画布的**尺寸对画面没有任何影响**，SCENE_ASPECT 只作用于 crop
//  （也就是纵向缩放）。它在数值上只经由这一处影响结果：
//        crop = W_real / (H_real × SCENE_ASPECT)
//
//  而「零变形」的条件是 uv 在两个方向上每像素增量相等：
//        横向 1.0 / W_real   ==   纵向 (1/crop) / H_real
//     →  crop = W_real / H_real
//     →  代入 crop 的定义得 SCENE_ASPECT = 1.0
//
//  取别的值就会引入各向异性：调小 = 纵向放得更大（云被竖向拉长），
//  调大 = 纵向压得更扁。想要更多/更少的云层细节就调它，但要知道自己在破坏比例。
//
//  和 fractal 的 0.5 不一样，那不矛盾：fractal 用的是 `u = .2*(u+u-v)/v.y`，
//  归一化除以的是 v.y（单一轴），所以它的取景由宽高比 v.x/v.y 决定；
//  这份除以的是 v.xy（两个轴），画布形状被约掉，只剩 crop 起作用。
//  每个 shader 的取景机制不同，所以旋钮值必须各自定 —— 这正是 SCENE_ASPECT
//  写在各自文件里、而不是做成全局设置的原因。
// ---------------------------------------------------------------------------
#define SCENE_ASPECT 1.0

vec3 sceneResolution() {
    return vec3(ubuf.iResolution.y * SCENE_ASPECT, ubuf.iResolution.y, 1.0);
}

// 让下面粘贴的 Shadertoy 正文里 iResolution / iTime 保持原样可用
#define iResolution sceneResolution()
#define iTime ubuf.iTime

// ---------------------------------------------------------------------------
//  以下为 Shadertoy 正文，原样粘贴，未作任何修改。
//  （这一份的形参名都没有和上面的 #define 撞车，所以不需要改名。）
// ---------------------------------------------------------------------------

float rand(vec2 n) {
    return fract(sin(dot(n, vec2(12.9898, 4.1414))) * 43758.5453);
}

float noise(vec2 p) {
    vec2 ip = floor(p);
    vec2 u = fract(p);
    u = u*u*(3.0-2.0*u);

    float res = mix(
        mix(rand(ip),rand(ip+vec2(1.0,0.0)),u.x),
        mix(rand(ip+vec2(0.0,1.0)),rand(ip+vec2(1.0,1.0)),u.x),u.y);
    return res*res;
}

const mat2 m2 = mat2(0.8,-0.6,0.6,0.8);

float fbm( in vec2 p ){
    float f = 0.0;
    f += 0.5000*noise( p ); p = m2*p*2.02;
    f += 0.2500*noise( p ); p = m2*p*2.03;
    f += 0.1250*noise( p ); p = m2*p*2.01;
    f += 0.0625*noise( p );

    return f/0.769;
}

float pattern( in vec2 p ) {
  vec2 q = vec2(fbm(p + vec2(0.0,0.0)));
  vec2 r = vec2( fbm( p + 4.0*q + vec2(1.7,9.2)));
  r+= iTime * 0.15;
  return fbm( p + 1.760*r );
}

void mainImage( out vec4 fragColor, in vec2 fragCoord ) {
      vec2 uv = fragCoord/iResolution.xy;

    uv *= 4.5; // Scale UV to make it nicer in that big screen !
      float displacement = pattern(uv);
      vec4 color = vec4(displacement * 1.2, 0.2, displacement * 5., 1.);

    color.a = min(color.r * 0.25, 1.); // Depth for CineShader
    fragColor = color;
}

// ---------------------------------------------------------------------------
//  包装层：Shadertoy 坐标 → 文字遮罩（与其它 shader 完全一致，照抄即可）
// ---------------------------------------------------------------------------
// ⚠ 正文已经定义完了，这里撤掉 iResolution 别名。
// 宏是在「使用处」展开的，所以上面的 mainImage 不受影响；而包装层若直接写
// ubuf.iResolution，会被展开成 ubuf.sceneResolution() → 编译错误。
// （fractal.frag 里有同样的一行；新加 shader 时最容易漏的就是这个。）
#undef iResolution

void main()
{
    vec2 realRes = ubuf.iResolution.xy;   // 组件真实像素尺寸

    // 纵向裁出与组件等宽高比的一条带（见文件顶部「取景」说明）。
    // 除以 SCENE_ASPECT 是为了在 A≠1 时也保持零变形：横向 uv 跨度是 1.0，
    // 纵向要匹配就得让可见纵向跨度 = H/W。
    float crop = realRes.x / (realRes.y * SCENE_ASPECT);

    vec2 centered = vec2(qt_TexCoord0.x,
                         (qt_TexCoord0.y - 0.5) / crop + 0.5);

    // Shadertoy 的 fragCoord：原点在左下、单位为像素；
    // qt_TexCoord0 原点在左上、范围 0..1 —— 所以 Y 要翻转。
    // 注意 #undef 之后不能再写 iResolution，必须显式取一次。
    vec3 virtRes = sceneResolution();
    vec2 fragCoord = vec2(centered.x, 1.0 - centered.y) * virtRes.xy;

    vec4 effectColor = vec4(0.0);
    mainImage(effectColor, fragCoord);

    // 本类 shader 是纯程序化的，不读输入纹理，所以必须自己用文字笔画的 alpha
    // 把特效裁到字上，否则整块矩形都会是特效。
    // ⚠ 这里必须用原始的 qt_TexCoord0，不能用上面的 centered：
    //   centered 只是给特效取景用的，拿它去取遮罩会和文字错位。
    float mask = texture(source, qt_TexCoord0).a;

    // Qt 场景图使用预乘 alpha：rgb 与 a 都要乘。
    // 必须 clamp —— Shadertoy 的输出常会超出 [0,1]（本例的 displacement * 5.
    // 可以远超 1），预乘 alpha 下 rgb > a 是非法值，会出现发光/过曝的伪影。
    //
    // 注：作者用 color.a 编码「深度」(min(color.r * 0.25, 1.))，这里按统一约定
    //     丢弃、只取 rgb —— 结构信息已经在 rgb 里了。若想让字的浓淡跟着深度走，
    //     把这里改成用 effectColor.a 即可（需要单独评估，会影响所有 shader 的观感）。
    vec3 col = clamp(effectColor.rgb, 0.0, 1.0);

    fragColor = vec4(col, 1.0) * mask * ubuf.qt_Opacity;
}
