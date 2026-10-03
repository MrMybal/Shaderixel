//@name Signed Detail
//@category Fusions
//@input Fond
//@input Calque
//@input Masque
//@halo 0
//@param Neutral float 0.5 -8 8 "Neutral"
//@param DetailStrength float 1 0 16 "Detail Strength"
//@param Opacity float 1 0 1 "Opacity"

// Imported preset (Blend / PBR): Adds signed detail around a configurable neutral value, useful for frequency separation and micro-detail.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyBackground = 0;
static const uint CyForeground = 1;
static const uint CyMask = 2;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Neutral, float DetailStrength, float Opacity)
{
    float4 bg=Texture2DSample(CyBackground,CyBackgroundSampler,UV);float4 fg=Texture2DSample(CyForeground,CyForegroundSampler,UV);float mask=Texture2DSample(CyMask,CyMaskSampler,UV).r;float3 detail=(fg.rgb-Neutral)*DetailStrength;float3 blend=bg.rgb+detail;float a=saturate(fg.a*mask*Opacity);return float4(lerp(bg.rgb,blend,a),a+bg.a*(1.0-a));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Neutral, DetailStrength, Opacity);
}
