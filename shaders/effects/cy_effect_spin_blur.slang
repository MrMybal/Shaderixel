//@name Spin Blur
//@category Traitements
//@input Image
//@halo full
//@param Amount float 0.15 -3.14159 3.14159 "Amount"
//@param Steps float 20 1 128 "Steps"
//@param HighQuality float 0 0 1 "High Quality"
//@param Dither float 1 0 1 "Dither"
//@param DitherAmount float 0.75 0 2 "Dither Amount"
//@param DitherSeed float 0 -1000 1000 "Dither Seed"
//@param Center float2 0.5 0.5 0 1 "Center"

// Imported preset (Effect / Blur & Denoise): Rotational blur around a configurable center with explicit sample quality.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Amount, float Steps, float HighQuality, float Dither, float DitherAmount, float DitherSeed, float3 Center)
{
    int Count=HighQuality>0.5?128:clamp((int)round(Steps),1,128);float2 DitherPixel=floor(UV*max(CyCanvasSize.xy,1.0));float2 P=UV-Center.xy;float4 Sum=0;[loop]for(int Index=0;Index<128;++Index){if(Index>=Count)break;float Jitter=(frac(sin(dot(DitherPixel+float2(Index*17.17+DitherSeed,Index*7.13-DitherSeed),float2(12.9898,78.233)))*43758.5453)-.5)*2.0*DitherAmount*Dither;float T=Count<=1?0.0:(Index/(Count-1.0)-0.5)*2.0;T+=Jitter*2.0/max((float)Count,1.0);float A=T*Amount;float C=cos(A);float S=sin(A);float2 Q=float2(C*P.x-S*P.y,S*P.x+C*P.y)+Center.xy;Sum+=Texture2DSample(CyInput,CyInputSampler,Q);}return Sum/max((float)Count,1.0);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Amount, Steps, HighQuality, Dither, DitherAmount, DitherSeed, float3(Center, 0.0));
}
