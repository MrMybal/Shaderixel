//@name Roughness Energy
//@category Fusions
//@input Fond
//@input Calque
//@input Masque
//@halo 0
//@param OutputAlpha float 0 0 1 "Output Alpha"
//@param Opacity float 1 0 1 "Opacity"
//@param BackgroundChannel color 1 0 0 0 "Background Channel"
//@param ForegroundChannel color 1 0 0 0 "Foreground Channel"
//@param OutputChannels color 1 1 1 0 "Output Channels"

// Imported preset (Blend / PBR): Blends roughness in squared perceptual space to avoid an unnaturally linear material transition.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyBackground = 0;
static const uint CyForeground = 1;
static const uint CyMask = 2;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float OutputAlpha, float Opacity, float4 BackgroundChannel, float4 ForegroundChannel, float3 OutputChannels)
{
    float4 bg=Texture2DSample(CyBackground,CyBackgroundSampler,UV);float4 fg=Texture2DSample(CyForeground,CyForegroundSampler,UV);float mask=Texture2DSample(CyMask,CyMaskSampler,UV).r;float rb=saturate(dot(bg,BackgroundChannel));float rf=saturate(dot(fg,ForegroundChannel));float a=saturate(fg.a*mask*Opacity);float rough=sqrt(max(lerp(rb*rb,rf*rf,a),0.0));float3 rgb=lerp(bg.rgb,rough.xxx,OutputChannels.rgb);float alpha=lerp(bg.a,rough,OutputAlpha*a);return float4(rgb,alpha);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), OutputAlpha, Opacity, BackgroundChannel, ForegroundChannel, OutputChannels.rgb);
}
