//@name Range
//@category Masques
//@input Image
//@halo 0
//@param Minimum float 0.25 0 1 "Minimum"
//@param Maximum float 0.75 0 1 "Maximum"
//@param Softness float 0.05 0.0001 0.5 "Softness"

// Imported preset (Mask / Mask): Selects a soft luminance range.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Minimum, float Maximum, float Softness)
{
    float3 c=Texture2DSample(CyInput,CyInputSampler,UV).rgb; float l=dot(c,float3(0.2126,0.7152,0.0722)); float v=smoothstep(Minimum-Softness,Minimum+Softness,l)*(1.0-smoothstep(Maximum-Softness,Maximum+Softness,l)); return float4(v.xxx,1);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Minimum, Maximum, Softness);
}
