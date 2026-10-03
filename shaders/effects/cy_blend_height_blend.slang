//@name Height Blend
//@category Fusions
//@input Fond
//@input Calque
//@input Masque
//@halo 0
//@param Contrast float 4 0 64 "Contrast"
//@param Bias float 0 -2 2 "Bias"
//@param Softness float 0.15 0.0001 2 "Softness"
//@param Opacity float 1 0 1 "Opacity"
//@param BackgroundHeightChannel color 0 0 0 1 "Background Height Channel"
//@param ForegroundHeightChannel color 0 0 0 1 "Foreground Height Channel"

// Imported preset (Blend / PBR): Height-aware material blend using configurable foreground/background channels, contrast, bias, and transition softness.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyBackground = 0;
static const uint CyForeground = 1;
static const uint CyMask = 2;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Contrast, float Bias, float Softness, float Opacity, float4 BackgroundHeightChannel, float4 ForegroundHeightChannel)
{
    float4 bg=Texture2DSample(CyBackground,CyBackgroundSampler,UV);float4 fg=Texture2DSample(CyForeground,CyForegroundSampler,UV);float mask=Texture2DSample(CyMask,CyMaskSampler,UV).r;float hb=dot(bg,BackgroundHeightChannel);float hf=dot(fg,ForegroundHeightChannel);float balance=(hf-hb)*Contrast+Bias;float height=smoothstep(-Softness,Softness,balance);float a=saturate(height*fg.a*mask*Opacity);return float4(lerp(bg.rgb,fg.rgb,a),a+bg.a*(1.0-a));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Contrast, Bias, Softness, Opacity, BackgroundHeightChannel, ForegroundHeightChannel);
}
