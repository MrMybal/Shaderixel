//@name HDR Soft Add
//@category Fusions
//@input Fond
//@input Calque
//@input Masque
//@halo 0
//@param Gain float 1 0 32 "Gain"
//@param WhitePoint float 4 0.01 64 "White Point"
//@param Rolloff float 0.35 0 1 "Rolloff"
//@param Opacity float 1 0 1 "Opacity"

// Imported preset (Blend / Utility): HDR-preserving soft additive blend with highlight rolloff and energy control.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyBackground = 0;
static const uint CyForeground = 1;
static const uint CyMask = 2;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Gain, float WhitePoint, float Rolloff, float Opacity)
{
    float4 bg=Texture2DSample(CyBackground,CyBackgroundSampler,UV);float4 fg=Texture2DSample(CyForeground,CyForegroundSampler,UV);float mask=Texture2DSample(CyMask,CyMaskSampler,UV).r;float3 energy=max(fg.rgb,0.0)*Gain;float3 added=bg.rgb+energy;float3 rolled=WhitePoint*(1.0-exp(-added/max(WhitePoint,.0001)));float3 blend=lerp(added,rolled,saturate(Rolloff));float a=saturate(fg.a*mask*Opacity);return float4(lerp(bg.rgb,blend,a),a+bg.a*(1.0-a));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Gain, WhitePoint, Rolloff, Opacity);
}
