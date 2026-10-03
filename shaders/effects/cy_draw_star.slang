//@name Star
//@category Dessin
//@generator
//@halo 0
//@param Points float 5 2 64 "Points"
//@param InnerRadius float 0.12 0.001 2 "Inner Radius"
//@param OuterRadius float 0.32 0.001 2 "Outer Radius"
//@param PointSharpness float 1 0.05 8 "Point Sharpness"
//@param Rotation float 0 -6.283 6.283 "Rotation"
//@param Aspect float 1 0.01 8 "Aspect"
//@param Softness float 0.004 0.0001 0.1 "Softness"
//@param Opacity float 1 0 1 "Opacity"
//@param Center float2 0.5 0.5 0 1 "Center"
//@param Color color 1 1 1 1 "Color"

// Imported preset (Draw / Shape & Gradient): Procedural star or flower shape with controllable point count.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Points, float InnerRadius, float OuterRadius, float PointSharpness, float Rotation, float Aspect, float Softness, float Opacity, float3 Center, float3 Color)
{
    float2 p=UV-Center.xy; p.x*=Aspect; float a=atan2(p.y,p.x)+Rotation; float count=max(Points,2.0); float wave=0.5+0.5*cos(a*count); float r=lerp(InnerRadius,OuterRadius,pow(wave,max(PointSharpness,0.05))); float mask=1.0-smoothstep(r,r+Softness,length(p)); return float4(Color.rgb,mask*Opacity);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Points, InnerRadius, OuterRadius, PointSharpness, Rotation, Aspect, Softness, Opacity, float3(Center, 0.0), Color.rgb);
}
