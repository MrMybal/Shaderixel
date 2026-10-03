//@name Bilateral Blur
//@category Traitements
//@input Image
//@halo full
//@param Radius float 12 0.25 512 "Radius"
//@param ColorSigma float 0.1 0.001 4 "Color Sigma"
//@param SpatialFalloff float 2 0.01 16 "Spatial Falloff"
//@param Steps float 24 1 128 "Steps"
//@param HighQuality float 0 0 1 "High Quality"
//@param Dither float 1 0 1 "Dither"
//@param DitherAmount float 0.75 0 2 "Dither Amount"
//@param DitherSeed float 0 -1000 1000 "Dither Seed"

// Imported preset (Effect / Blur & Denoise): Edge-preserving blur with spatial radius, color threshold, and sample quality.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float ColorSigma, float SpatialFalloff, float Steps, float HighQuality, float Dither, float DitherAmount, float DitherSeed)
{
    int Count=HighQuality>0.5?128:clamp((int)round(Steps),1,128);float2 DitherPixel=floor(UV*max(CyCanvasSize.xy,1.0));float4 center=Texture2DSample(CyInput,CyInputSampler,UV);float4 sum=center;float weights=1.0;[loop]for(int Index=0;Index<128;Index++){if(Index>=Count)break;float Jitter=(frac(sin(dot(DitherPixel+float2(Index*17.17+DitherSeed,Index*7.13-DitherSeed),float2(12.9898,78.233)))*43758.5453)-.5)*2.0*DitherAmount*Dither;float f=saturate((Index+.5+Jitter)/Count);float a=Index*2.39996323;float2 o=float2(cos(a),sin(a))*sqrt(f)*Radius/max(CyCanvasSize.xy,1.0);float4 tap=Texture2DSample(CyInput,CyInputSampler,UV+o);float w=exp(-dot(tap.rgb-center.rgb,tap.rgb-center.rgb)/max(ColorSigma*ColorSigma,.000001))*exp(-f*SpatialFalloff);sum+=tap*w;weights+=w;}return sum/max(weights,.00001);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, ColorSigma, SpatialFalloff, Steps, HighQuality, Dither, DitherAmount, DitherSeed);
}
