//@name Grid
//@category CyTexture Dessin
//@generator
//@halo 0
//@param Divisions float 16 1 512 "Divisions"
//@param LineWidth float 0.035 0.001 0.49 "Line Width"
//@param Softness float 0.01 0.0001 0.2 "Softness"
//@param LineOpacity float 1 0 1 "Line Opacity"
//@param BackgroundOpacity float 1 0 1 "Background Opacity"
//@param LineColor color 1 1 1 1 "Line Color"
//@param BackgroundColor color 0 0 0 1 "Background Color"

// Imported from CyTexture (Draw / Pattern): Procedural grid with independent background and line colors.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Divisions, float LineWidth, float Softness, float LineOpacity, float BackgroundOpacity, float3 LineColor, float3 BackgroundColor)
{
    float2 g=abs(frac(UV*max(Divisions,1.0)+0.5)-0.5); float gridLine=1.0-smoothstep(LineWidth,LineWidth+Softness,min(g.x,g.y)); return float4(lerp(BackgroundColor.rgb,LineColor.rgb,gridLine),lerp(BackgroundOpacity,LineOpacity,gridLine));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Divisions, LineWidth, Softness, LineOpacity, BackgroundOpacity, LineColor.rgb, BackgroundColor.rgb);
}
