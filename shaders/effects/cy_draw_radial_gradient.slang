//@name Radial Gradient
//@category Dessin
//@generator
//@halo 0
//@param Radius float 0.35 0.001 2 "Radius"
//@param Softness float 0.15 0 1 "Softness"
//@param InnerOpacity float 1 0 1 "Inner Opacity"
//@param OuterOpacity float 0 0 1 "Outer Opacity"
//@param Center float2 0.5 0.5 0 1 "Center"
//@param InnerColor color 1 1 1 1 "Inner Color"
//@param OuterColor color 0 0 0 0 "Outer Color"

// Imported preset (Draw / Shape & Gradient): Configurable radial gradient useful for shapes and vignettes.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float Softness, float InnerOpacity, float OuterOpacity, float3 Center, float3 InnerColor, float3 OuterColor)
{
    float d=length(UV-Center.xy); float t=smoothstep(max(Radius-Softness,0.0),Radius,d); return float4(lerp(InnerColor,OuterColor,t),lerp(InnerOpacity,OuterOpacity,t));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, Softness, InnerOpacity, OuterOpacity, float3(Center, 0.0), InnerColor.rgb, OuterColor.rgb);
}
