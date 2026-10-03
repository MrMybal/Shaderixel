//@name Aurora Bands
//@category Dessin
//@generator
//@halo 0
//@param Frequency float 8 0.1 64 "Frequency"
//@param Phase float 0 -20 20 "Phase"
//@param Amplitude float 0.08 0 0.5 "Amplitude"
//@param Width float 0.08 0.001 0.5 "Width"
//@param Sharpness float 1.4 0.05 8 "Sharpness"
//@param Striation float 18 1 256 "Striation"
//@param StriationAmount float 0.55 0 1 "Striation Amount"
//@param Horizon float 0.52 0 1 "Horizon"
//@param HorizonFalloff float 0.65 0.001 2 "Horizon Falloff"
//@param Intensity float 3 0 64 "Intensity"
//@param Opacity float 1 0 1 "Opacity"
//@param ColorA color 0.05 2 0.45 1 "Color A"
//@param ColorB color 0.4 0.08 2.5 1 "Color B"

// Imported preset (Draw / Light & Space): Layered HDR aurora ribbons with turbulence, glow, and horizon falloff.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Frequency, float Phase, float Amplitude, float Width, float Sharpness, float Striation, float StriationAmount, float Horizon, float HorizonFalloff, float Intensity, float Opacity, float3 ColorA, float3 ColorB)
{
    float2 p=UV; float wave=0.5+Amplitude*(sin(p.x*Frequency+Phase)+0.45*sin(p.x*Frequency*2.17-Phase*1.3)); float d=abs(p.y-wave); float ribbon=exp(-d/max(Width,0.0001)); float striation=0.55+0.45*sin((p.x*Striation+p.y*7.0+Phase)*6.283); float horizon=pow(saturate(1.0-abs(p.y-Horizon)/max(HorizonFalloff,0.0001)),2.0); float v=pow(saturate(ribbon*lerp(1.0,striation,StriationAmount)*horizon),max(Sharpness,0.05)); float3 rgb=lerp(ColorA.rgb,ColorB.rgb,saturate(p.x+v*0.25))*v*Intensity; return float4(rgb,saturate(v*Opacity));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Frequency, Phase, Amplitude, Width, Sharpness, Striation, StriationAmount, Horizon, HorizonFalloff, Intensity, Opacity, ColorA.rgb, ColorB.rgb);
}
