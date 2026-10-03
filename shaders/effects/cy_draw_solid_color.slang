//@name Solid Color
//@category CyTexture Dessin
//@generator
//@halo 0
//@param Opacity float 1 0 1 "Opacity"
//@param Color color 1 1 1 1 "Color"

// Imported from CyTexture (Draw / Shape & Gradient): Fills the complete layer with an HDR color.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Opacity, float3 Color)
{
    return float4(Color,Opacity);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Opacity, Color.rgb);
}
