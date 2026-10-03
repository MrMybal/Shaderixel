//@name Solarize
//@category CyTexture Effets
//@input Image
//@halo 0
//@param Threshold float 0.5 0 1 "Threshold"
//@param Strength float 1 0 1 "Strength"

// Imported from CyTexture (Effect / Color & Tone): Photographic solarization with a movable threshold and blend strength.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Threshold, float Strength)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float3 v=lerp(s.rgb,1.0-s.rgb,step(Threshold,s.rgb)); return float4(lerp(s.rgb,v,saturate(Strength)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Threshold, Strength);
}
