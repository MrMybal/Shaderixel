//@name Scanlines
//@category Traitements
//@input Image
//@halo 0
//@param Frequency float 540 1 8192 "Frequency"
//@param Phase float 0 -10 10 "Phase"
//@param Sharpness float 3 0.1 32 "Sharpness"
//@param Intensity float 0.25 0 1 "Intensity"
//@param LineColor color 0 0 0 1 "Line Color"

// Imported preset (Effect / Stylize): CRT-style scanlines with phase and tint.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Frequency, float Phase, float Sharpness, float Intensity, float3 LineColor)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float scanline=0.5+0.5*cos((UV.y*Frequency+Phase)*6.2831853); scanline=pow(scanline,max(Sharpness,0.1)); float3 v=s.rgb*lerp(1.0,LineColor.rgb,scanline*Intensity); return float4(v,s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Frequency, Phase, Sharpness, Intensity, LineColor.rgb);
}
