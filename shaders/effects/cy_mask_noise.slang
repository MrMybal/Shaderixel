//@name Noise
//@category Masques
//@input Image
//@halo 0
//@param Scale float 256 1 4096 "Scale"
//@param Seed float 0 -1000 1000 "Seed"
//@param Threshold float 0.35 0 1 "Threshold"
//@param Contrast float 4 0 32 "Contrast"
//@param UseSource float 0 0 1 "Use Source"

// Imported preset (Mask / Mask): Multiplies source luminance by procedural noise.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Scale, float Seed, float Threshold, float Contrast, float UseSource)
{
    float l=dot(Texture2DSample(CyInput,CyInputSampler,UV).rgb,float3(.2126,.7152,.0722)); float n=frac(sin(dot(floor(UV*max(Scale,1.0))+Seed,float2(12.9898,78.233)))*43758.5453); float v=saturate((n-Threshold)*Contrast)*lerp(1.0,l,saturate(UseSource)); return float4(v.xxx,1);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Scale, Seed, Threshold, Contrast, UseSource);
}
