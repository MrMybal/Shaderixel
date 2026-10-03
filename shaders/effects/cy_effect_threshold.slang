//@name Threshold
//@category CyTexture Effets
//@input Image
//@halo 0
//@param Threshold float 0.5 0 1 "Threshold"

// Imported from CyTexture (Effect / Color & Tone): Creates a hard luminance threshold while retaining source alpha.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Threshold)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float v=step(Threshold,dot(s.rgb,float3(0.2126,0.7152,0.0722))); return float4(v.xxx,s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Threshold);
}
