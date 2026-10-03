//@name Stripes
//@category Dessin
//@generator
//@halo 0
//@param Frequency float 16 1 512 "Frequency"
//@param Angle float 0 -6.283 6.283 "Angle"
//@param Phase float 0 -10 10 "Phase"
//@param Duty float 0.5 0.01 0.99 "Duty"
//@param Softness float 0.01 0.0001 0.25 "Softness"
//@param OpacityA float 1 0 1 "Opacity A"
//@param OpacityB float 1 0 1 "Opacity B"
//@param ColorA color 1 1 1 1 "Color A"
//@param ColorB color 0 0 0 1 "Color B"

// Imported preset (Draw / Shape & Gradient): Rotatable antialiased stripe generator.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Frequency, float Angle, float Phase, float Duty, float Softness, float OpacityA, float OpacityB, float3 ColorA, float3 ColorB)
{
    float2 p=UV-0.5; float c=cos(Angle); float s=sin(Angle); float u=dot(p,float2(c,s))*Frequency+Phase; float v=frac(u); float edge=max(Softness,0.0001); float m=1.0-smoothstep(Duty-edge,Duty+edge,v); return float4(lerp(ColorB.rgb,ColorA.rgb,m),lerp(OpacityB,OpacityA,m));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Frequency, Angle, Phase, Duty, Softness, OpacityA, OpacityB, ColorA.rgb, ColorB.rgb);
}
