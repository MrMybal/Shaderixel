//@name Drop Shadow
//@category Traitements
//@input Image
//@halo full
//@param Radius float 12 0 256 "Radius"
//@param Opacity float 0.75 0 1 "Opacity"
//@param Steps float 16 1 128 "Steps"
//@param HighQuality float 0 0 1 "High Quality"
//@param Dither float 1 0 1 "Dither"
//@param DitherAmount float 0.75 0 2 "Dither Amount"
//@param DitherSeed float 0 -1000 1000 "Dither Seed"
//@param Offset float2 16 16 -2 2 "Offset"
//@param ShadowColor color 0 0 0 1 "Shadow Color"

// Imported preset (Effect / Color & Tone): Creates a soft offset shadow from the source alpha while retaining the source.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float Opacity, float Steps, float HighQuality, float Dither, float DitherAmount, float DitherSeed, float3 Offset, float3 ShadowColor)
{
    int Count=HighQuality>0.5?128:clamp((int)round(Steps),1,128);float2 DitherPixel=floor(UV*max(CyCanvasSize.xy,1.0));float4 s=Texture2DSample(CyInput,CyInputSampler,UV);float alpha=0;[loop]for(int Index=0;Index<128;Index++){if(Index>=Count)break;float Jitter=(frac(sin(dot(DitherPixel+float2(Index*17.17+DitherSeed,Index*7.13-DitherSeed),float2(12.9898,78.233)))*43758.5453)-.5)*2.0*DitherAmount*Dither;float f=saturate((Index+.5+Jitter)/Count);float a=Index*2.39996323;float2 blur=float2(cos(a),sin(a))*sqrt(f)*Radius/max(CyCanvasSize.xy,1.0);float2 off=Offset.xy/max(CyCanvasSize.xy,1.0);alpha+=Texture2DSample(CyInput,CyInputSampler,UV-off+blur).a;}alpha=alpha/max((float)Count,1.0)*Opacity*(1.0-s.a);return float4(s.rgb+ShadowColor.rgb*alpha,s.a+alpha);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, Opacity, Steps, HighQuality, Dither, DitherAmount, DitherSeed, float3(Offset, 0.0), ShadowColor.rgb);
}
