//@name Motion Blur
//@category CyTexture Effets
//@input Image
//@halo full
//@param Angle float 0 -6.283 6.283 "Angle"
//@param Distance float 32 0 2048 "Distance"
//@param Steps float 24 1 128 "Steps"
//@param HighQuality float 0 0 1 "High Quality"
//@param Dither float 1 0 1 "Dither"
//@param DitherAmount float 0.75 0 2 "Dither Amount"
//@param DitherSeed float 0 -1000 1000 "Dither Seed"

// Imported from CyTexture (Effect / Blur & Denoise): Linear motion blur with angle, distance, and GPU sample quality.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Angle, float Distance, float Steps, float HighQuality, float Dither, float DitherAmount, float DitherSeed)
{
    int Count=HighQuality>0.5?128:clamp((int)round(Steps),1,128);float2 DitherPixel=floor(UV*max(CyCanvasSize.xy,1.0));float2 dir=float2(cos(Angle),sin(Angle))*Distance/max(CyCanvasSize.xy,1.0);float4 sum=0;[loop]for(int Index=0;Index<128;Index++){if(Index>=Count)break;float Jitter=(frac(sin(dot(DitherPixel+float2(Index*17.17+DitherSeed,Index*7.13-DitherSeed),float2(12.9898,78.233)))*43758.5453)-.5)*2.0*DitherAmount*Dither;float t=Count<=1?0.0:((Index+Jitter)/(Count-1.0)-.5);sum+=Texture2DSample(CyInput,CyInputSampler,UV+dir*t);}return sum/max((float)Count,1.0);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Angle, Distance, Steps, HighQuality, Dither, DitherAmount, DitherSeed);
}
