#version 440

// ============================================================================
//  Oran7 歌词文字特效 —— fractal (Shadertoy)
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
//  取景：按 1:1 渲染图案，再纵向裁出正中间的一条带铺满组件
//
//  ① 为什么不能直接用 Shadertoy 的原始 16:9 取景
//
//     试过了，两侧会立刻发黑。这个 shader 的亮区**天生集中在 u = 0 附近**：
//     正文里那项 `sin(1.5*u/(.5-dot(u,u)) - 9.*u.yx + t)`，
//     其中 `- 9.*u.yx` 随 |u| 线性增长 → |u| 越大 sin 振荡越快 →
//     19 次迭代的累加 `o` 相互抵消 → 越靠外越暗。
//     再叠上结尾的径向暗角 `- dot(u, u) / 250.`，两侧就彻底黑了。
//
//     ⚠ 这不是分辨率问题。我在两种画布量级下都验证过两侧发黑：
//       组件真实尺寸(dot(v,v)≈8000)和固定 720p(dot(v,v)≈216万)，
//       结果一样。所以「调大画布量级就能让图案铺满」是错的，
//       我先前那个「黑边是分辨率失配的假象」的判断也不成立。
//
//  ② 解法：把镜头推近到亮区内部
//
//     可见横向跨度 = u.x ∈ ±0.2A，实测 A 超过约 1.0 就开始压边、
//     A=2.0 时两侧全黑。取 0.5（相对 1:1 放大 2 倍）可以稳稳落在亮区内。
//
//     代价是看不到原始 16:9 取景的全貌 —— 但那是这个 shader 的亮区
//     本身就只有那么点大，和「原始不原始」无关：
//     16:9 画面的两侧在 Shadertoy 上同样是暗的。
//
//  ③ 横向铺满、纵向裁剪
//
//     一旦选定「横向铺满」，纵向裁剪就是**被强制的** ——
//     要保持 1:1，可见区域的长宽比必须等于胶囊的长宽比（约 10:1）。
//     crop 由组件真实尺寸实时算出，胶囊宽度随歌词长短变化时等式始终成立。
// ---------------------------------------------------------------------------
#define SCENE_ASPECT 0.5

vec3 sceneResolution() {
    return vec3(ubuf.iResolution.y * SCENE_ASPECT, ubuf.iResolution.y, 1.0);
}

// 让下面粘贴的 Shadertoy 正文里 iResolution / iTime 保持原样可用
#define iResolution sceneResolution()
#define iTime ubuf.iTime

// ---------------------------------------------------------------------------
//  以下为 Shadertoy 正文，原样粘贴。
//  移植时只做了一件事：
//   把 for 循环里那个**未初始化**的循环变量 `i` 补了初值 0。
//   原版是 `for (float a = .5, t = iTime, i; ++i < 19.; ...)` —— `i` 没有初值，
//   而循环条件第一件事就是 `++i`。GLSL 里未初始化局部变量的值是未定义的，
//   Shadertoy 上能跑是因为浏览器/driver 恰好清零；桌面 GLSL 不保证，
//   一旦拿到大值，循环一次都不执行 → 画面退化成一整块纯色。
//   （若你粘贴的 shader 也有这个写法，记得同样补初值。）
// ---------------------------------------------------------------------------

// -13 thanks to Nguyen2007 ⚡
void mainImage( out vec4 o, vec2 u )
{
    vec2 v = iResolution.xy;
         u = .2*(u+u-v)/v.y;

    vec4 z = o = vec4(1,2,3,0);

    for (float a = .5, t = iTime, i = 0.;
         ++i < 19.;
         o += (1. + cos(z+t))
            / length((1.+i*dot(v,v))
                   * sin(1.5*u/(.5-dot(u,u)) - 9.*u.yx + t))
         )
        v = cos(++t - 7.*u*pow(a += .03, i)) - 5.*u,
        // use stanh here if shader has black artifacts
        //   vvvv
        u += tanh(40. * dot(u *= mat2(cos(i + .02*t - z.wxzw*11.))
                           ,u)
                      * cos(1e2*u.yx + t)) / 2e2
           + .2 * a * u
           + cos(4./exp(dot(o,o)/1e2) + t) / 3e2;

     o = 25.6 / (min(o, 13.) + 164. / o)
       - dot(u, u) / 250.;
}




// Original [436]
// （作者保留的旧版本，被 /* */ 注释掉了，不参与编译；那个 //*/ 结尾是用来
//   切换注释的惯用写法。）
/*
void mainImage( out vec4 o, vec2 u )
{
    vec2 v = iResolution.xy,
         w,
         k = u = .2*(u+u-v)/v.y;

    o = vec4(1,2,3,0);

    for (float a = .5, t = iTime, i;
         ++i < 19.;
         o += (1.+ cos(vec4(0,1,3,0)+t))
           / length((1.+i*dot(v,v)) * sin(w*3.-9.*u.yx+t))
         )
        v = cos(++t - 7.*u*pow(a += .03, i)) - 5.*u,
        u *= mat2(cos(i+t*.02 - vec4(0,11,33,0))),
        u += .005 * tanh(40.*dot(u,u)*cos(1e2*u.yx+t))
           + .2 * a * u
           + .003 * cos(t+4.*exp(-.01*dot(o,o))),
        w = u / (1. -2.*dot(u,u));

    o = pow(o = 1.-sqrt(exp(-o*o*o/2e2)), .3*o/o)
      - dot(k-=u,k) / 250.;
}
//*/

// ---------------------------------------------------------------------------
//  包装层：Shadertoy 坐标 → 文字遮罩（与 octgrams.frag 完全一致，照抄即可）
// ---------------------------------------------------------------------------
// ⚠ 正文已经定义完了，这里撤掉 iResolution 别名。
// 宏是在「使用处」展开的，所以上面的 mainImage 不受影响；
// 而包装层若直接写 ubuf.iResolution 会被展开成 ubuf.sceneResolution() → 编译错误。
#undef iResolution

void main()
{
    vec2 realRes = ubuf.iResolution.xy;   // 组件真实像素尺寸
    vec3 virtRes = sceneResolution();     // 虚拟画布（SCENE_ASPECT=1.0 时即 1:1）

    // 纵向裁出与组件等宽高比的一条带（见文件顶部「取景」说明）。
    // 除以 SCENE_ASPECT 是为了在 A≠1 时也保持零变形：横向 u 跨度是 ±0.2A，
    // 纵向要匹配就得让可见纵向跨度 = ±0.2A·(H/W)。
    float crop = realRes.x / (realRes.y * SCENE_ASPECT);

    vec2 centered = vec2(qt_TexCoord0.x,
                         (qt_TexCoord0.y - 0.5) / crop + 0.5);

    // Shadertoy 的 fragCoord：原点在左下、单位为像素；
    // qt_TexCoord0 原点在左上、范围 0..1 —— 所以 Y 要翻转。
    vec2 fragCoord = vec2(centered.x, 1.0 - centered.y) * virtRes.xy;

    vec4 effectColor = vec4(0.0);
    mainImage(effectColor, fragCoord);

    // 本类 shader 是纯程序化的，不读输入纹理，所以必须自己用文字笔画的 alpha
    // 把特效裁到字上，否则整块矩形都会是特效。
    // ⚠ 这里必须用原始的 qt_TexCoord0，不能用上面的 centered：
    //   centered 只是给特效取景用的，拿它去取遮罩会和文字错位。
    float mask = texture(source, qt_TexCoord0).a;

    // Qt 场景图使用预乘 alpha：rgb 与 a 都要乘。
    // 必须 clamp —— Shadertoy 的输出常会超出 [0,1]（本例 o 可以到十几），
    // 预乘 alpha 下 rgb > a 是非法值，会出现发光/过曝的伪影。
    vec3 col = clamp(effectColor.rgb, 0.0, 1.0);

    fragColor = vec4(col, 1.0) * mask * ubuf.qt_Opacity;
}
