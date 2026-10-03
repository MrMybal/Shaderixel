//@name Gamma
//@category Traitements
//@input Image
//@halo 0
//@param Gamma float 1 0.05 8 "Gamma"
//@param Strength float 1 0 1 "Strength"

// Imported preset (Effect / Color & Tone): Per-channel gamma correction with blend strength.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Gamma, float Strength)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float3 v=pow(max(s.rgb,0.0),1.0/max(Gamma,0.001)); return float4(lerp(s.rgb,v,saturate(Strength)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Gamma, Strength);
}
