//@name Match Color
//@category CyTexture Effets
//@input Image
//@halo 0
//@param LuminanceStrength float 1 0 1 "Luminance Strength"
//@param ColorStrength float 1 0 1 "Color Strength"
//@param Strength float 1 0 1 "Strength"
//@param SourceAverage color 0.5 0.5 0.5 1 "Source Average"
//@param TargetAverage color 0.5 0.5 0.5 1 "Target Average"

// Imported from CyTexture (Effect / Color & Tone): Matches the image toward a target average color with luminance and color strengths.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float LuminanceStrength, float ColorStrength, float Strength, float3 SourceAverage, float3 TargetAverage)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float sl=max(dot(SourceAverage.rgb,float3(.2126,.7152,.0722)),0.0001); float tl=dot(TargetAverage.rgb,float3(.2126,.7152,.0722)); float3 lum=s.rgb*(tl/sl); float3 chroma=s.rgb*TargetAverage.rgb/max(SourceAverage.rgb,0.0001); float3 v=lerp(s.rgb,lum,saturate(LuminanceStrength)); v=lerp(v,chroma,saturate(ColorStrength)); return float4(lerp(s.rgb,v,saturate(Strength)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), LuminanceStrength, ColorStrength, Strength, SourceAverage.rgb, TargetAverage.rgb);
}
