//@name Circle
//@category CyTexture Dessin
//@generator
//@halo 0
//@param Radius float 0.3 0.001 2 "Radius"
//@param Thickness float 0.04 0.001 1 "Thickness"
//@param Softness float 0.005 0.0001 0.25 "Softness"
//@param Aspect float 1 0.01 8 "Aspect"
//@param Outline float 0 0 1 "Outline"
//@param Opacity float 1 0 1 "Opacity"
//@param Center float2 0.5 0.5 0 1 "Center"
//@param Color color 1 1 1 1 "Color"

// Imported from CyTexture (Draw / Shape & Gradient): Antialiased filled or outlined circle with HDR color.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float Thickness, float Softness, float Aspect, float Outline, float Opacity, float3 Center, float3 Color)
{
    float2 p=UV-Center.xy; p.x*=Aspect; float d=length(p); float outer=1.0-smoothstep(Radius,Radius+Softness,d); float inner=1.0-smoothstep(max(Radius-Thickness,0.0),max(Radius-Thickness,0.0)+Softness,d); float a=lerp(outer,saturate(outer-inner),saturate(Outline)); return float4(Color.rgb,a*Opacity);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, Thickness, Softness, Aspect, Outline, Opacity, float3(Center, 0.0), Color.rgb);
}
