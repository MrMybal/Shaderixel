//@name Camouflage
//@category Dessin
//@generator
//@halo 0
//@param Scale float 3.5 0.1 64 "Scale"
//@param Seed float 0 -1000 1000 "Seed"
//@param Contrast float 1.2 0 8 "Contrast"
//@param ThresholdA float 0.32 0 1 "Threshold A"
//@param ThresholdB float 0.53 0 1 "Threshold B"
//@param ThresholdC float 0.74 0 1 "Threshold C"
//@param Opacity float 1 0 1 "Opacity"
//@param ColorA color 0.035 0.055 0.025 1 "Color A"
//@param ColorB color 0.12 0.18 0.06 1 "Color B"
//@param ColorC color 0.32 0.28 0.12 1 "Color C"
//@param ColorD color 0.08 0.07 0.045 1 "Color D"

// Imported preset (Draw / Surface): Multi-scale organic camouflage regions with four colors, warped boundaries, and seamless UV input.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Scale, float Seed, float Contrast, float ThresholdA, float ThresholdB, float ThresholdC, float Opacity, float3 ColorA, float3 ColorB, float3 ColorC, float3 ColorD)
{
    float2 p=UV*max(Scale,.01)+Seed;float a=sin(p.x*2.1+sin(p.y*1.7))+sin(p.y*2.7-p.x*.8);float b=sin(p.x*5.3-p.y*3.1+Seed)+sin((p.x+p.y)*4.7);float c=sin(p.x*11.3+p.y*7.9-Seed);float n=saturate((a*.28+b*.15+c*.07)*Contrast+.5);float3 rgb=ColorA.rgb;if(n>ThresholdA)rgb=ColorB.rgb;if(n>ThresholdB)rgb=ColorC.rgb;if(n>ThresholdC)rgb=ColorD.rgb;return float4(rgb,Opacity);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Scale, Seed, Contrast, ThresholdA, ThresholdB, ThresholdC, Opacity, ColorA.rgb, ColorB.rgb, ColorC.rgb, ColorD.rgb);
}
