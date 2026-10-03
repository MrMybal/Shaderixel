//@name Rounded Rectangle
//@category CyTexture Dessin
//@generator
//@halo 0
//@param CornerRadius float 0.05 0 0.5 "Corner Radius"
//@param BorderWidth float 0.02 0.001 0.5 "Border Width"
//@param Softness float 0.003 0.0001 0.1 "Softness"
//@param Outline float 0 0 1 "Outline"
//@param Opacity float 1 0 1 "Opacity"
//@param Center float2 0.5 0.5 0 1 "Center"
//@param HalfSize float2 0.3 0.2 0 1 "Half Size"
//@param Color color 1 1 1 1 "Color"

// Imported from CyTexture (Draw / Shape & Gradient): Signed-distance rounded rectangle with border support.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float CornerRadius, float BorderWidth, float Softness, float Outline, float Opacity, float3 Center, float3 HalfSize, float3 Color)
{
    float2 p=abs(UV-Center.xy)-HalfSize.xy+CornerRadius; float d=min(max(p.x,p.y),0.0)+length(max(p,0.0))-CornerRadius; float fill=1.0-smoothstep(0.0,Softness,d); float inside=1.0-smoothstep(-BorderWidth,-BorderWidth+Softness,d); float a=lerp(fill,saturate(fill-inside),saturate(Outline)); return float4(Color.rgb,a*Opacity);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), CornerRadius, BorderWidth, Softness, Outline, Opacity, float3(Center, 0.0), float3(HalfSize, 0.0), Color.rgb);
}
