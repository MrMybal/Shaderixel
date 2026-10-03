//@name SDF Arc
//@category Dessin
//@generator
//@halo 0
//@param Aspect float 1 0.1 8 "Aspect"
//@param Radius float 0.3 0.001 2 "Radius"
//@param Thickness float 0.018 0.001 0.5 "Thickness"
//@param Softness float 0.004 0.0001 0.1 "Softness"
//@param MidAngle float 0 -6.283 6.283 "Mid Angle"
//@param ArcAngle float 3.4 0.01 6.283 "Arc Angle"
//@param AngularSoftness float 0.01 0.0001 0.5 "Angular Softness"
//@param GlowRadius float 0.08 0.001 1 "Glow Radius"
//@param GlowFalloff float 2 0.1 16 "Glow Falloff"
//@param GlowIntensity float 0.7 0 16 "Glow Intensity"
//@param Intensity float 2 0 64 "Intensity"
//@param Opacity float 1 0 1 "Opacity"
//@param Center float2 0.5 0.5 0 1 "Center"
//@param Color color 0.1 1.1 2.5 1 "Color"

// Imported preset (Draw / Shape & Gradient): Antialiased signed-distance arc with round caps, glow, and HDR output.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Aspect, float Radius, float Thickness, float Softness, float MidAngle, float ArcAngle, float AngularSoftness, float GlowRadius, float GlowFalloff, float GlowIntensity, float Intensity, float Opacity, float3 Center, float3 Color)
{
    float2 p=UV-Center.xy; p.x*=Aspect; float angle=atan2(p.y,p.x); float radius=length(p); float da=abs(atan2(sin(angle-MidAngle),cos(angle-MidAngle))); float arcMask=1.0-smoothstep(ArcAngle*0.5,ArcAngle*0.5+AngularSoftness,da); float ring=1.0-smoothstep(Thickness,Thickness+Softness,abs(radius-Radius)); float capAngle=ArcAngle*0.5; float2 c0=float2(cos(MidAngle-capAngle),sin(MidAngle-capAngle))*Radius; float2 c1=float2(cos(MidAngle+capAngle),sin(MidAngle+capAngle))*Radius; float caps=max(1.0-smoothstep(Thickness,Thickness+Softness,length(p-c0)),1.0-smoothstep(Thickness,Thickness+Softness,length(p-c1))); float m=max(ring*arcMask,caps); float glow=pow(saturate(1.0-abs(radius-Radius)/max(GlowRadius,0.0001)),GlowFalloff)*arcMask; float3 rgb=Color.rgb*(m+glow*GlowIntensity)*Intensity; return float4(rgb,saturate(max(m,glow)*Opacity));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Aspect, Radius, Thickness, Softness, MidAngle, ArcAngle, AngularSoftness, GlowRadius, GlowFalloff, GlowIntensity, Intensity, Opacity, float3(Center, 0.0), Color.rgb);
}
