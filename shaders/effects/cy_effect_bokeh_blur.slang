//@name Bokeh Blur
//@category CyTexture Effets
//@input Image
//@halo full
//@param Radius float 18 0 512 "Radius"
//@param Blades float 6 3 12 "Blades"
//@param Rotation float 0 -6.283 6.283 "Rotation"
//@param Threshold float 1 0 16 "Threshold"
//@param HighlightBoost float 1 0 16 "Highlight Boost"
//@param Steps float 24 1 128 "Steps"
//@param HighQuality float 0 0 1 "High Quality"
//@param Dither float 1 0 1 "Dither"
//@param DitherAmount float 0.75 0 2 "Dither Amount"
//@param DitherSeed float 0 -1000 1000 "Dither Seed"

// Imported from CyTexture (Effect / Blur & Denoise): Polygonal aperture bokeh blur with blades, rotation, highlight boost, and quality.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float Blades, float Rotation, float Threshold, float HighlightBoost, float Steps, float HighQuality, float Dither, float DitherAmount, float DitherSeed)
{
    int Count=HighQuality>0.5?128:clamp((int)round(Steps),1,128);float2 DitherPixel=floor(UV*max(CyCanvasSize.xy,1.0));float BladeCount=max(Blades,3.0);float4 Sum=0;float WeightSum=0;[loop]for(int Index=0;Index<128;++Index){if(Index>=Count)break;float Jitter=(frac(sin(dot(DitherPixel+float2(Index*17.17+DitherSeed,Index*7.13-DitherSeed),float2(12.9898,78.233)))*43758.5453)-.5)*2.0*DitherAmount*Dither;float T=saturate((Index+0.5+Jitter)/Count);float A=Index*2.39996323+Rotation;float Polygon=cos(3.14159265/BladeCount)/max(cos(fmod(A+3.14159265/BladeCount,6.2831853/BladeCount)-3.14159265/BladeCount),0.001);float2 Offset=float2(cos(A),sin(A))*sqrt(T)*Radius*Polygon/max(CyCanvasSize.xy,1.0);float4 Tap=Texture2DSample(CyInput,CyInputSampler,UV+Offset);float Weight=1.0+max(dot(Tap.rgb,float3(.2126,.7152,.0722))-Threshold,0.0)*HighlightBoost;Sum+=Tap*Weight;WeightSum+=Weight;}return Sum/max(WeightSum,0.00001);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, Blades, Rotation, Threshold, HighlightBoost, Steps, HighQuality, Dither, DitherAmount, DitherSeed);
}
