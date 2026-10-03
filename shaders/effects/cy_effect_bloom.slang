//@name Bloom
//@category CyTexture Effets
//@input Image
//@halo full
//@param Threshold float 1 0 16 "Threshold"
//@param Radius float 12 0.25 256 "Radius"
//@param Intensity float 1 0 32 "Intensity"
//@param Steps float 24 1 128 "Steps"
//@param HighQuality float 0 0 1 "High Quality"
//@param Dither float 1 0 1 "Dither"
//@param DitherAmount float 0.75 0 2 "Dither Amount"
//@param DitherSeed float 0 -1000 1000 "Dither Seed"
//@param Tint color 1 1 1 1 "Tint"

// Imported from CyTexture (Effect / Color & Tone): HDR threshold bloom with adjustable radius, deterministic dither, and up to 128 samples.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Threshold, float Radius, float Intensity, float Steps, float HighQuality, float Dither, float DitherAmount, float DitherSeed, float3 Tint)
{
    int Count=HighQuality>0.5?128:clamp((int)round(Steps),1,128);float2 DitherPixel=floor(UV*max(CyCanvasSize.xy,1.0));float4 Source=Texture2DSample(CyInput,CyInputSampler,UV);float3 BloomSum=0;[loop]for(int Index=0;Index<128;++Index){if(Index>=Count)break;float Jitter=(frac(sin(dot(DitherPixel+float2(Index*17.17+DitherSeed,Index*7.13-DitherSeed),float2(12.9898,78.233)))*43758.5453)-.5)*2.0*DitherAmount*Dither;float F=saturate((Index+0.5+Jitter)/Count);float A=Index*2.39996323;float2 Offset=float2(cos(A),sin(A))*sqrt(F)*Radius/max(CyCanvasSize.xy,1.0);BloomSum+=max(Texture2DSample(CyInput,CyInputSampler,UV+Offset).rgb-Threshold,0.0);}return float4(Source.rgb+BloomSum/max((float)Count,1.0)*Intensity*Tint.rgb,Source.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Threshold, Radius, Intensity, Steps, HighQuality, Dither, DitherAmount, DitherSeed, Tint.rgb);
}
