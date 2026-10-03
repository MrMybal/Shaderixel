//@name Checker
//@category Dessin
//@generator
//@halo 0
//@param Tiling float 8 1 256 "Tiling"
//@param OpacityA float 1 0 1 "Opacity A"
//@param OpacityB float 1 0 1 "Opacity B"
//@param ColorA color 0 0 0 1 "Color A"
//@param ColorB color 1 1 1 1 "Color B"

// Imported preset (Draw / Shape & Gradient): Procedural checker pattern with independent colors.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Tiling, float OpacityA, float OpacityB, float3 ColorA, float3 ColorB)
{
    float2 cell=floor(UV*max(Tiling,1.0)); float v=fmod(cell.x+cell.y,2.0); return float4(lerp(ColorA,ColorB,v),lerp(OpacityA,OpacityB,v));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Tiling, OpacityA, OpacityB, ColorA.rgb, ColorB.rgb);
}
