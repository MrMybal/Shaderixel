//@name Threshold
//@category Masques
//@input Image
//@halo 0
//@param Threshold float 0.5 0 1 "Threshold"
//@param Softness float 0.05 0 0.5 "Softness"
//@param Invert float 0 0 1 "Invert"

// Imported preset (Mask / Mask): Soft luminance threshold mask.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Threshold, float Softness, float Invert)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float l=dot(s.rgb,float3(0.2126,0.7152,0.0722)); float v=smoothstep(Threshold-Softness,Threshold+Softness,l); if(Invert>0.5) v=1.0-v; return float4(v.xxx,1);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Threshold, Softness, Invert);
}
