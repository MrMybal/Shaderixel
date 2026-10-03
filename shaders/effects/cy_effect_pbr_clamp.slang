//@name PBR Clamp
//@category CyTexture Effets
//@input Image
//@halo 0
//@param AlphaMinimum float 0 0 1 "Alpha Minimum"
//@param AlphaMaximum float 1 0 1 "Alpha Maximum"
//@param ClampAlpha float 0 0 1 "Clamp Alpha"
//@param Minimum color 0 0 0 0 "Minimum"
//@param Maximum color 1 1 1 0 "Maximum"
//@param Channels color 1 1 1 0 "Channels"

// Imported from CyTexture (Effect / PBR): Clamps selected linear PBR channels to physically plausible per-channel ranges.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float AlphaMinimum, float AlphaMaximum, float ClampAlpha, float3 Minimum, float3 Maximum, float3 Channels)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV);float3 clamped=clamp(s.rgb,Minimum.rgb,Maximum.rgb);float3 rgb=lerp(s.rgb,clamped,Channels.rgb);float alpha=lerp(s.a,clamp(s.a,AlphaMinimum,AlphaMaximum),ClampAlpha);return float4(rgb,alpha);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), AlphaMinimum, AlphaMaximum, ClampAlpha, Minimum.rgb, Maximum.rgb, Channels.rgb);
}
