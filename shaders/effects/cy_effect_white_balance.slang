//@name White Balance
//@category CyTexture Effets
//@input Image
//@halo 0
//@param Temperature float 0 -1 1 "Temperature"
//@param Tint float 0 -1 1 "Tint"
//@param Strength float 1 0 1 "Strength"

// Imported from CyTexture (Effect / Color & Tone): HDR-safe temperature and tint correction with a neutral default.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Temperature, float Tint, float Strength)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float3 balance=float3(1.0+Temperature*0.12-Tint*0.04,1.0+Tint*0.08,1.0-Temperature*0.12-Tint*0.04); float3 v=max(s.rgb,0.0)*max(balance,0.001); return float4(lerp(s.rgb,v,saturate(Strength)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Temperature, Tint, Strength);
}
