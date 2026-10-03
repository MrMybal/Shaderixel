//@name Alpha Feather
//@category Masques
//@input Image
//@halo 0
//@param Threshold float 0.5 0 1 "Threshold"
//@param Feather float 0.1 0.0001 0.5 "Feather"
//@param Gamma float 1 0.01 8 "Gamma"

// Imported preset (Mask / Mask): Remaps and feathers the source alpha channel.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Threshold, float Feather, float Gamma)
{
    float a=Texture2DSample(CyInput,CyInputSampler,UV).a; float v=smoothstep(Threshold-Feather,Threshold+Feather,a); v=pow(saturate(v),max(Gamma,0.01)); return float4(v.xxx,1);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Threshold, Feather, Gamma);
}
