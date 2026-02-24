// Fullscreen triangle vertex shader (no vertex buffer needed)
// Compile: vs_5_0, entry: main

struct VSOut
{
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

VSOut main(uint vid : SV_VertexID)
{
    // Fullscreen triangle (covers entire screen)
    float2 p[3] = {
        float2(-1.0, -1.0),
        float2(-1.0,  3.0),
        float2( 3.0, -1.0)
    };

    // UV mapping for fullscreen triangle
    // Note: This UV setup matches the triangle above
    float2 t[3] = {
        float2(0.0, 1.0),
        float2(0.0,-1.0),
        float2(2.0, 1.0)
    };

    VSOut o;
    o.pos = float4(p[vid], 0.0, 1.0);
    o.uv  = t[vid];
    return o;
}

