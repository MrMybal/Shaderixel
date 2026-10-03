//@name Anamorphic Lens Flare
//@category Dessin
//@generator
//@halo 0
//@param Aspect float 1 0.1 8 "Aspect"
//@param CoreFalloff float 18 0.1 128 "Core Falloff"
//@param CoreIntensity float 6 0 64 "Core Intensity"
//@param StreakWidth float 0.006 0.0001 0.25 "Streak Width"
//@param StreakLength float 0.75 0.001 4 "Streak Length"
//@param StreakFalloff float 2 0.1 16 "Streak Falloff"
//@param StreakIntensity float 4 0 64 "Streak Intensity"
//@param ColorSplit float 0.004 0 0.1 "Color Split"
//@param Opacity float 1 0 1 "Opacity"
//@param Center float2 0.5 0.5 0 1 "Center"
//@param CoreColor color 1 0.75 0.35 1 "Core Color"
//@param StreakColor color 0.18 0.45 1 1 "Streak Color"

// Imported preset (Draw / Light & Space): Wide cinematic anamorphic flare with horizontal streaks and spectral split.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Aspect, float CoreFalloff, float CoreIntensity, float StreakWidth, float StreakLength, float StreakFalloff, float StreakIntensity, float ColorSplit, float Opacity, float3 Center, float3 CoreColor, float3 StreakColor)
{
    float2 p=UV-Center.xy; float core=exp(-length(float2(p.x*Aspect,p.y))*CoreFalloff)*CoreIntensity; float streak=exp(-abs(p.y)/max(StreakWidth,0.0001))*pow(saturate(1.0-abs(p.x)/max(StreakLength,0.0001)),StreakFalloff)*StreakIntensity; float blue=exp(-abs(p.y+ColorSplit)/max(StreakWidth,0.0001)); float red=exp(-abs(p.y-ColorSplit)/max(StreakWidth,0.0001)); float3 rgb=CoreColor.rgb*core+StreakColor.rgb*streak*float3(red,1.0,blue); float a=saturate(max(max(rgb.r,rgb.g),rgb.b)*Opacity); return float4(rgb*Opacity,a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Aspect, CoreFalloff, CoreIntensity, StreakWidth, StreakLength, StreakFalloff, StreakIntensity, ColorSplit, Opacity, float3(Center, 0.0), CoreColor.rgb, StreakColor.rgb);
}
