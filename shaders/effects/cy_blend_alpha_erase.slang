//@name Alpha Erase
//@category Fusions
//@input Fond
//@input Calque
//@input Masque
//@halo 0
//@param Opacity float 1 0 1 "Opacity"
//@param ReplaceRGB float 0 0 1 "Replace RGB"
//@param EraseColor color 0 0 0 1 "Erase Color"

// Imported preset (Blend / Utility): Uses foreground alpha and mask as a nondestructive eraser while optionally retaining RGB data.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyBackground = 0;
static const uint CyForeground = 1;
static const uint CyMask = 2;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Opacity, float ReplaceRGB, float3 EraseColor)
{
    float4 bg=Texture2DSample(CyBackground,CyBackgroundSampler,UV);float4 fg=Texture2DSample(CyForeground,CyForegroundSampler,UV);float mask=Texture2DSample(CyMask,CyMaskSampler,UV).r;float erase=saturate(fg.a*mask*Opacity);float outAlpha=bg.a*(1.0-erase);float3 rgb=lerp(bg.rgb,EraseColor.rgb,erase*ReplaceRGB);return float4(rgb,outAlpha);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Opacity, ReplaceRGB, EraseColor.rgb);
}
