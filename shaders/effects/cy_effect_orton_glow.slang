//@name Orton Glow
//@category Traitements
//@input Image
//@halo full
//@param Radius float 16 0.25 512 "Radius"
//@param Brightness float 1.35 0 8 "Brightness"
//@param ScreenMode float 1 0 1 "Screen Mode"
//@param Strength float 0.55 0 1 "Strength"
//@param Steps float 24 1 128 "Steps"
//@param HighQuality float 0 0 1 "High Quality"
//@param Dither float 1 0 1 "Dither"
//@param DitherAmount float 0.75 0 2 "Dither Amount"
//@param DitherSeed float 0 -1000 1000 "Dither Seed"

// Imported preset (Effect / Blur & Denoise): Dreamy Orton process combining a soft high-key image with the original through screen or multiply.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float Brightness, float ScreenMode, float Strength, float Steps, float HighQuality, float Dither, float DitherAmount, float DitherSeed)
{
    int Count=HighQuality>0.5?128:clamp((int)round(Steps),1,128);float2 DitherPixel=floor(UV*max(CyCanvasSize.xy,1.0));float2 t=Radius/max(CyCanvasSize.xy,1.0);float4 c=Texture2DSample(CyInput,CyInputSampler,UV);float4 blur=0;[loop]for(int Index=0;Index<128;Index++){if(Index>=Count)break;float Jitter=(frac(sin(dot(DitherPixel+float2(Index*17.17+DitherSeed,Index*7.13-DitherSeed),float2(12.9898,78.233)))*43758.5453)-.5)*2.0*DitherAmount*Dither;float f=saturate((Index+.5+Jitter)/Count);float a=Index*2.39996323;blur+=Texture2DSample(CyInput,CyInputSampler,UV+float2(cos(a),sin(a))*sqrt(f)*t);}blur/=max((float)Count,1.0);float3 high=blur.rgb*Brightness;float3 screen=1.0-(1.0-c.rgb)*(1.0-high);float3 multiply=c.rgb*high;float3 blend=lerp(multiply,screen,saturate(ScreenMode));return float4(lerp(c.rgb,blend,saturate(Strength)),c.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, Brightness, ScreenMode, Strength, Steps, HighQuality, Dither, DitherAmount, DitherSeed);
}
