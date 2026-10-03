//@name Wood Grain
//@category CyTexture Dessin
//@generator
//@halo 0
//@param Scale float 7 0.1 128 "Scale"
//@param Rotation float 0 -6.283 6.283 "Rotation"
//@param Seed float 0 -1000 1000 "Seed"
//@param WarpFrequency float 2.8 0.1 64 "Warp Frequency"
//@param WarpAmount float 0.22 0 2 "Warp Amount"
//@param RingWobble float 0.08 0 1 "Ring Wobble"
//@param RingSharpness float 2.2 0.1 16 "Ring Sharpness"
//@param PoreScale float 38 1 1024 "Pore Scale"
//@param PoreAmount float 0.08 0 1 "Pore Amount"
//@param PoreStrength float 0.25 0 2 "Pore Strength"
//@param Opacity float 1 0 1 "Opacity"
//@param EarlyWood color 0.12 0.035 0.008 1 "Early Wood"
//@param LateWood color 0.55 0.22 0.045 1 "Late Wood"

// Imported from CyTexture (Draw / Surface): Seamless procedural wood rings with warped fibers, pores, and adjustable early/late wood color.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Scale, float Rotation, float Seed, float WarpFrequency, float WarpAmount, float RingWobble, float RingSharpness, float PoreScale, float PoreAmount, float PoreStrength, float Opacity, float3 EarlyWood, float3 LateWood)
{
    float2 p=UV*max(Scale,0.01);float c=cos(Rotation),s=sin(Rotation);p=float2(c*p.x-s*p.y,s*p.x+c*p.y);float warp=sin(p.y*WarpFrequency+Seed)+.5*sin(p.y*WarpFrequency*2.17-p.x*.7+Seed);float rings=frac((p.x+warp*WarpAmount)+sin(p.y*1.7)*RingWobble);float band=pow(saturate(1.0-abs(rings-.5)*2.0),RingSharpness);float pore=frac(sin(dot(floor(p*PoreScale)+Seed,float2(12.9898,78.233)))*43758.5453);float pores=smoothstep(1.0-PoreAmount,1.0,pore)*PoreStrength;float3 rgb=lerp(EarlyWood.rgb,LateWood.rgb,saturate(band+pores));return float4(rgb,Opacity);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Scale, Rotation, Seed, WarpFrequency, WarpAmount, RingWobble, RingSharpness, PoreScale, PoreAmount, PoreStrength, Opacity, EarlyWood.rgb, LateWood.rgb);
}
