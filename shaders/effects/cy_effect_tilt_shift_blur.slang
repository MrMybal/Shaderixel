//@name Tilt Shift Blur
//@category Traitements
//@input Image
//@halo full
//@param Radius float 24 0 512 "Radius"
//@param FocusAngle float 0 -6.283 6.283 "Focus Angle"
//@param FocusWidth float 0.08 0 1 "Focus Width"
//@param Softness float 0.2 0.001 1 "Softness"
//@param Steps float 20 1 128 "Steps"
//@param HighQuality float 0 0 1 "High Quality"
//@param Dither float 1 0 1 "Dither"
//@param DitherAmount float 0.75 0 2 "Dither Amount"
//@param DitherSeed float 0 -1000 1000 "Dither Seed"
//@param Center float2 0.5 0.5 0 1 "Center"

// Imported preset (Effect / Blur & Denoise): Focus-band blur with adjustable angle, transition, radius, and quality.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float FocusAngle, float FocusWidth, float Softness, float Steps, float HighQuality, float Dither, float DitherAmount, float DitherSeed, float3 Center)
{
    int Count=HighQuality>0.5?128:clamp((int)round(Steps),1,128);float2 DitherPixel=floor(UV*max(CyCanvasSize.xy,1.0));float2 FocusNormal=float2(-sin(FocusAngle),cos(FocusAngle));float Distance=abs(dot(UV-Center.xy,FocusNormal));float LocalRadius=Radius*saturate((Distance-FocusWidth)/max(Softness,0.0001));float4 Sum=0;[loop]for(int Index=0;Index<128;++Index){if(Index>=Count)break;float Jitter=(frac(sin(dot(DitherPixel+float2(Index*17.17+DitherSeed,Index*7.13-DitherSeed),float2(12.9898,78.233)))*43758.5453)-.5)*2.0*DitherAmount*Dither;float F=saturate((Index+0.5+Jitter)/Count);float A=Index*2.39996323;float2 Offset=float2(cos(A),sin(A))*sqrt(F)*LocalRadius/max(CyCanvasSize.xy,1.0);Sum+=Texture2DSample(CyInput,CyInputSampler,UV+Offset);}return Sum/max((float)Count,1.0);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, FocusAngle, FocusWidth, Softness, Steps, HighQuality, Dither, DitherAmount, DitherSeed, float3(Center, 0.0));
}
