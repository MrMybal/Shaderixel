//@name Gaussian Blur
//@category CyTexture Effets
//@input Image
//@halo full
//@param Radius float 16 0 512 "Radius"
//@param Sigma float 6 0.01 256 "Sigma"
//@param Angle float 0 -6.283 6.283 "Angle"
//@param Steps float 24 1 128 "Steps"
//@param HighQuality float 0 0 1 "High Quality"
//@param Dither float 1 0 1 "Dither"
//@param DitherAmount float 0.75 0 2 "Dither Amount"
//@param DitherSeed float 0 -1000 1000 "Dither Seed"

// Imported from CyTexture (Effect / Blur & Denoise): High-quality directional Gaussian blur; stack horizontal and vertical instances for a full 2D Gaussian.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float Sigma, float Angle, float Steps, float HighQuality, float Dither, float DitherAmount, float DitherSeed)
{
    int Count=HighQuality>0.5?128:clamp((int)round(Steps),1,128);float2 DitherPixel=floor(UV*max(CyCanvasSize.xy,1.0));float2 Direction=float2(cos(Angle)/max(CyCanvasSize.x,1.0),sin(Angle)/max(CyCanvasSize.y,1.0));float4 Sum=0;float WeightSum=0;[loop]for(int Index=0;Index<128;++Index){if(Index>=Count)break;float Jitter=(frac(sin(dot(DitherPixel+float2(Index*17.17+DitherSeed,Index*7.13-DitherSeed),float2(12.9898,78.233)))*43758.5453)-.5)*2.0*DitherAmount*Dither;float T=Count<=1?0.0:(Index/(Count-1.0)-0.5)*2.0;T+=Jitter*2.0/max((float)Count,1.0);float Pixel=T*Radius;float Weight=exp(-0.5*Pixel*Pixel/max(Sigma*Sigma,0.0001));Sum+=Texture2DSample(CyInput,CyInputSampler,UV+Direction*Pixel)*Weight;WeightSum+=Weight;}return Sum/max(WeightSum,0.00001);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, Sigma, Angle, Steps, HighQuality, Dither, DitherAmount, DitherSeed);
}
