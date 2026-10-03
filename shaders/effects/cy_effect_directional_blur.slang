//@name Directional Blur
//@category CyTexture Effets
//@input Image
//@halo full
//@param Angle float 0 -6.283 6.283 "Angle"
//@param Distance float 12 0 512 "Distance"
//@param Steps float 16 1 128 "Steps"
//@param HighQuality float 0 0 1 "High Quality"
//@param Dither float 1 0 1 "Dither"
//@param DitherAmount float 0.75 0 2 "Dither Amount"
//@param DitherSeed float 0 -1000 1000 "Dither Seed"

// Imported from CyTexture (Effect / Blur & Denoise): Directional motion blur with up to 128 steps, sample-radius dithering, and high-quality mode.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Angle, float Distance, float Steps, float HighQuality, float Dither, float DitherAmount, float DitherSeed)
{
    int Count=HighQuality>0.5?128:clamp((int)round(Steps),1,128);float2 DitherPixel=floor(UV*max(CyCanvasSize.xy,1.0));float2 Direction=float2(cos(Angle)/max(CyCanvasSize.x,1.0),sin(Angle)/max(CyCanvasSize.y,1.0))*Distance;float4 Sum=0;[loop]for(int Index=0;Index<128;++Index){if(Index>=Count)break;float Jitter=(frac(sin(dot(DitherPixel+float2(Index*17.17+DitherSeed,Index*7.13-DitherSeed),float2(12.9898,78.233)))*43758.5453)-.5)*2.0*DitherAmount*Dither;float T=Count<=1?0.0:(Index/(Count-1.0)-0.5)*2.0;T+=Jitter*2.0/max((float)Count,1.0);Sum+=Texture2DSample(CyInput,CyInputSampler,UV+Direction*T);}return Sum/max((float)Count,1.0);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Angle, Distance, Steps, HighQuality, Dither, DitherAmount, DitherSeed);
}
