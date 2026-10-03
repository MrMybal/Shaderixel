//@name PBR Channel Merge
//@category CyTexture Fusions
//@input Fond
//@input Calque
//@input Masque
//@halo 0
//@param ForegroundAlpha float 0 0 1 "Foreground Alpha"
//@param Opacity float 1 0 1 "Opacity"
//@param ForegroundChannels color 1 1 1 0 "Foreground Channels"

// Imported from CyTexture (Blend / PBR): Merges arbitrary foreground RGB and alpha channels into a packed PBR texture through a shared mask.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyBackground = 0;
static const uint CyForeground = 1;
static const uint CyMask = 2;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float ForegroundAlpha, float Opacity, float3 ForegroundChannels)
{
    float4 bg=Texture2DSample(CyBackground,CyBackgroundSampler,UV);float4 fg=Texture2DSample(CyForeground,CyForegroundSampler,UV);float mask=Texture2DSample(CyMask,CyMaskSampler,UV).r;float weight=saturate(mask*Opacity);float3 selected=lerp(bg.rgb,fg.rgb,saturate(ForegroundChannels.rgb)*weight);float alpha=lerp(bg.a,fg.a,saturate(ForegroundAlpha)*weight);return float4(selected,alpha);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), ForegroundAlpha, Opacity, ForegroundChannels.rgb);
}
