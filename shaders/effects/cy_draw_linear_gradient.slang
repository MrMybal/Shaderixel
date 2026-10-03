//@name Linear Gradient
//@category Dessin
//@generator
//@halo 0
//@param Angle float 0 -3.14159 3.14159 "Angle"
//@param Width float 1 0.01 4 "Width"
//@param OpacityA float 1 0 1 "Opacity A"
//@param OpacityB float 1 0 1 "Opacity B"
//@param ColorA color 0 0 0 1 "Color A"
//@param ColorB color 1 1 1 1 "Color B"

// Imported preset (Draw / Shape & Gradient): Two-color directional linear gradient.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Angle, float Width, float OpacityA, float OpacityB, float3 ColorA, float3 ColorB)
{
    float2 p=UV-0.5; float c=cos(Angle); float s=sin(Angle); float t=saturate(dot(p,float2(c,s))/max(Width,0.0001)+0.5); return float4(lerp(ColorA,ColorB,t),lerp(OpacityA,OpacityB,t));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Angle, Width, OpacityA, OpacityB, ColorA.rgb, ColorB.rgb);
}
