//@name High Pass
//@category Traitements
//@input Image
//@halo full
//@param Radius float 8 0.25 256 "Radius"
//@param Strength float 1 0 8 "Strength"
//@param Steps float 24 1 128 "Steps"
//@param HighQuality float 0 0 1 "High Quality"
//@param Dither float 1 0 1 "Dither"
//@param DitherAmount float 0.75 0 2 "Dither Amount"
//@param DitherSeed float 0 -1000 1000 "Dither Seed"

// Imported preset (Effect / Detail): High-pass detail isolation with up to 128 samples, deterministic radius dithering, and a one-click high-quality mode.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float Strength, float Steps, float HighQuality, float Dither, float DitherAmount, float DitherSeed)
{
    int Count=HighQuality>0.5?128:clamp((int)round(Steps),1,128);float2 DitherPixel=floor(UV*max(CyCanvasSize.xy,1.0));float4 src=Texture2DSample(CyInput,CyInputSampler,UV);float4 avg=0;[loop]for(int Index=0;Index<128;++Index){if(Index>=Count)break;float Jitter=(frac(sin(dot(DitherPixel+float2(Index*17.17+DitherSeed,Index*7.13-DitherSeed),float2(12.9898,78.233)))*43758.5453)-.5)*2.0*DitherAmount*Dither;float f=saturate((Index+0.5+Jitter)/Count);float a=Index*2.39996323;float2 o=float2(cos(a),sin(a))*sqrt(f)*Radius/max(CyCanvasSize.xy,1.0);avg+=Texture2DSample(CyInput,CyInputSampler,UV+o);}avg/=max((float)Count,1.0);return float4(0.5+(src.rgb-avg.rgb)*Strength,src.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, Strength, Steps, HighQuality, Dither, DitherAmount, DitherSeed);
}
