#version 440

// ============================================================================
//  Oran7 歌词文字特效 —— Octgrams (Shadertoy)
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
    float iTime;              // 秒
} ubuf;
// ========================= 契约头结束 =======================================

// ---------------------------------------------------------------------------
//  观感旋钮：虚拟画布宽高比
//
//  歌词胶囊很扁（约 10:1）。若直接拿真实尺寸当 iResolution，主程序里的
//      p = (fragCoord*2 - iResolution.xy) / min(iResolution.x, iResolution.y)
//  会把视野横向扇开到 ±5，大片留白、场景只剩中间一条。
//  这里把画布虚拟成 SCENE_ASPECT:1 —— 场景等比缩放、水平裁切，而不是被拉扁。
//
//  本 shader 只用 iResolution 算归一化坐标，不用它的模长（不像 fractal 里的
//  dot(v,v)），所以画布的绝对像素数不影响观感，只需关心宽高比。
//  想看到更宽的场景就调大这个值（16:9 即 1.778）。
// ---------------------------------------------------------------------------
#define SCENE_ASPECT 2.0

vec3 sceneResolution() {
    return vec3(ubuf.iResolution.y * SCENE_ASPECT, ubuf.iResolution.y, 1.0);
}

// 让下面粘贴的 Shadertoy 正文里 iResolution / iTime 保持原样可用
#define iResolution sceneResolution()
#define iTime ubuf.iTime

// ---------------------------------------------------------------------------
//  以下为 Shadertoy 正文，原样粘贴。
//  移植时只做了两件事：
//   1) 删掉 `precision highp float;`（GLSL ES 的东西，#version 440 不需要）
//   2) 把 box_set / map 里那个从未被使用的形参 `iTime` 改名为 `t`
//      —— 因为上面的 #define 会把它展开成语法错误。这是唯一需要留意的点：
//      若你粘贴的 shader 里有形参叫 iTime / iResolution，把它改个名即可。
// ---------------------------------------------------------------------------

float gTime = 0.;
const float REPEAT = 5.0;

mat2 rot(float a) {
    float c = cos(a), s = sin(a);
    return mat2(c,s,-s,c);
}

float sdBox( vec3 p, vec3 b )
{
    vec3 q = abs(p) - b;
    return length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0);
}

float box(vec3 pos, float scale) {
    pos *= scale;
    float base = sdBox(pos, vec3(.4,.4,.1)) /1.5;
    pos.xy *= 5.;
    pos.y -= 3.5;
    pos.xy *= rot(.75);
    float result = -base;
    return result;
}

float box_set(vec3 pos, float t) {
    vec3 pos_origin = pos;
    pos = pos_origin;
    pos .y += sin(gTime * 0.4) * 2.5;
    pos.xy *=   rot(.8);
    float box1 = box(pos,2. - abs(sin(gTime * 0.4)) * 1.5);
    pos = pos_origin;
    pos .y -=sin(gTime * 0.4) * 2.5;
    pos.xy *=   rot(.8);
    float box2 = box(pos,2. - abs(sin(gTime * 0.4)) * 1.5);
    pos = pos_origin;
    pos .x +=sin(gTime * 0.4) * 2.5;
    pos.xy *=   rot(.8);
    float box3 = box(pos,2. - abs(sin(gTime * 0.4)) * 1.5);
    pos = pos_origin;
    pos .x -=sin(gTime * 0.4) * 2.5;
    pos.xy *=   rot(.8);
    float box4 = box(pos,2. - abs(sin(gTime * 0.4)) * 1.5);
    pos = pos_origin;
    pos.xy *=   rot(.8);
    float box5 = box(pos,.5) * 6.;
    pos = pos_origin;
    float box6 = box(pos,.5) * 6.;
    float result = max(max(max(max(max(box1,box2),box3),box4),box5),box6);
    return result;
}

float map(vec3 pos, float t) {
    vec3 pos_origin = pos;
    float box_set1 = box_set(pos, t);

    return box_set1;
}


void mainImage( out vec4 fragColor, in vec2 fragCoord ) {
    vec2 p = (fragCoord.xy * 2. - iResolution.xy) / min(iResolution.x, iResolution.y);
    vec3 ro = vec3(0., -0.2 ,iTime * 4.);
    vec3 ray = normalize(vec3(p, 1.5));
    ray.xy = ray.xy * rot(sin(iTime * .03) * 5.);
    ray.yz = ray.yz * rot(sin(iTime * .05) * .2);
    float t = 0.1;
    vec3 col = vec3(0.);
    float ac = 0.0;


    for (int i = 0; i < 99; i++){
        vec3 pos = ro + ray * t;
        pos = mod(pos-2., 4.) -2.;
        gTime = iTime -float(i) * 0.01;

        float d = map(pos, iTime);

        d = max(abs(d), 0.01);
        ac += exp(-d*23.);

        t += d* 0.55;
    }

    col = vec3(ac * 0.02);

    col +=vec3(0.,0.2 * abs(sin(iTime)),0.5 + sin(iTime) * 0.2);


    fragColor = vec4(col ,1.0 - t * (0.02 + 0.02 * sin (iTime)));
}

// ---------------------------------------------------------------------------
//  包装层：Shadertoy 坐标 → 文字遮罩
//  这段也请照抄到每个新 shader 文件里，只有 SCENE_ASPECT 需要按需调整。
// ---------------------------------------------------------------------------
void main()
{
    // Shadertoy 的 fragCoord：原点在左下、单位为像素；
    // qt_TexCoord0 原点在左上、范围 0..1 —— 所以 Y 要翻转。
    vec2 fragCoord = vec2(qt_TexCoord0.x, 1.0 - qt_TexCoord0.y) * iResolution.xy;

    vec4 effectColor = vec4(0.0);
    mainImage(effectColor, fragCoord);

    // 本类 shader 是纯程序化的，不读输入纹理，所以必须自己用文字笔画的 alpha
    // 把特效裁到字上，否则整块矩形都会是特效。
    float mask = texture(source, qt_TexCoord0).a;

    // Qt 场景图使用预乘 alpha：rgb 与 a 都要乘。
    // 必须 clamp —— Shadertoy 的输出常会超出 [0,1]（本例的 ac 累加可以到几十），
    // 预乘 alpha 下 rgb > a 是非法值，会出现发光/过曝的伪影。
    vec3 col = clamp(effectColor.rgb, 0.0, 1.0);

    fragColor = vec4(col, 1.0) * mask * ubuf.qt_Opacity;
}
