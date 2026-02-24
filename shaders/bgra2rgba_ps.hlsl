// BGRA -> RGBA swizzle pixel shader
// Compile: ps_5_0, entry: main

Texture2D    tex0 : register(t0);
SamplerState smp0 : register(s0);

float4 main(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
    return tex0.Sample(smp0, uv);
}


