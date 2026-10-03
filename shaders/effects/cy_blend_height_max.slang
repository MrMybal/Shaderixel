//@name Height Max
//@category CyTexture Fusions
//@input Fond
//@input Calque
//@input Masque
//@halo 0
//@param HeightBias float 0 -2 2 "Height Bias"
//@param Softness float 0.05 0.0001 1 "Softness"
//@param Opacity float 1 0 1 "Opacity"
//@param BackgroundHeightChannel color 0 0 0 1 "Background Height Channel"
//@param ForegroundHeightChannel color 0 0 0 1 "Foreground Height Channel"

// Imported from CyTexture (Blend / PBR): Selects the material with the greater packed height while preserving a controllable soft transition.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyBackground = 0;
static const uint CyForeground = 1;
static const uint CyMask = 2;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float HeightBias, float Softness, float Opacity, float4 BackgroundHeightChannel, float4 ForegroundHeightChannel)
{
    float4 bg=Texture2DSample(CyBackground,CyBackgroundSampler,UV);float4 fg=Texture2DSample(CyForeground,CyForegroundSampler,UV);float mask=Texture2DSample(CyMask,CyMaskSampler,UV).r;float hb=dot(bg,BackgroundHeightChannel);float hf=dot(fg,ForegroundHeightChannel)+HeightBias;float selector=smoothstep(-Softness,Softness,hf-hb);float a=saturate(selector*mask*Opacity);float4 combined=lerp(bg,fg,a);return float4(combined.rgb,lerp(bg.a,max(bg.a,fg.a),a));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), HeightBias, Softness, Opacity, BackgroundHeightChannel, ForegroundHeightChannel);
}
