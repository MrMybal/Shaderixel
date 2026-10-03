//@name Negation
//@category Fusions
//@input Fond
//@input Calque
//@input Masque
//@halo 0
//@param Opacity float 1 0 1 "Opacity"

// Imported preset (Blend / Compositing): Photographic negation blend.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyBackground = 0;
static const uint CyForeground = 1;
static const uint CyMask = 2;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Opacity)
{
    float4 bg=Texture2DSample(CyBackground,CyBackgroundSampler,UV); float4 fg=Texture2DSample(CyForeground,CyForegroundSampler,UV); float m=Texture2DSample(CyMask,CyMaskSampler,UV).r; float3 b=1.0-abs(1.0-bg.rgb-fg.rgb); float a=saturate(fg.a*m*Opacity); return float4(lerp(bg.rgb,b,a),a+bg.a*(1.0-a));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Opacity);
}
