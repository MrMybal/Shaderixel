//@name Anamorphic Bloom
//@category CyTexture Effets
//@input Image
//@halo full
//@param Threshold float 1 0 16 "Threshold"
//@param Radius float 48 0.25 1024 "Radius"
//@param Falloff float 2 0.01 16 "Falloff"
//@param Intensity float 2 0 64 "Intensity"
//@param Steps float 32 1 128 "Steps"
//@param HighQuality float 0 0 1 "High Quality"
//@param Dither float 1 0 1 "Dither"
//@param DitherAmount float 0.75 0 2 "Dither Amount"
//@param DitherSeed float 0 -1000 1000 "Dither Seed"
//@param Tint color 0.2 0.45 1 1 "Tint"

// Imported from CyTexture (Effect / Color & Tone): Quality-controlled horizontal threshold streak for cinematic anamorphic highlights.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Threshold, float Radius, float Falloff, float Intensity, float Steps, float HighQuality, float Dither, float DitherAmount, float DitherSeed, float3 Tint)
{
    int Count=HighQuality>0.5?128:clamp((int)round(Steps),1,128);float2 DitherPixel=floor(UV*max(CyCanvasSize.xy,1.0));float4 Source=Texture2DSample(CyInput,CyInputSampler,UV);float3 BloomSum=0;float WeightSum=0;[loop]for(int Index=0;Index<128;++Index){if(Index>=Count)break;float Jitter=(frac(sin(dot(DitherPixel+float2(Index*17.17+DitherSeed,Index*7.13-DitherSeed),float2(12.9898,78.233)))*43758.5453)-.5)*2.0*DitherAmount*Dither;float T=Count<=1?0.0:((Index+Jitter)/(Count-1.0)-0.5)*2.0;float Weight=exp(-abs(T)*Falloff);float2 Offset=float2(T*Radius/max(CyCanvasSize.x,1.0),0);BloomSum+=max(Texture2DSample(CyInput,CyInputSampler,UV+Offset).rgb-Threshold,0.0)*Weight;WeightSum+=Weight;}return float4(Source.rgb+BloomSum/max(WeightSum,0.00001)*Intensity*Tint.rgb,Source.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Threshold, Radius, Falloff, Intensity, Steps, HighQuality, Dither, DitherAmount, DitherSeed, Tint.rgb);
}
