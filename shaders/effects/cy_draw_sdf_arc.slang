//@name SDF Arc
//@category Dessin
//@graph {"name":"SDF Arc","category":"Dessin","inputs":0,"premultiplied":false,"nextId":19,"nodes":[{"id":1,"op":"uv","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"canvassize","pos":[40,130],"in":[],"value":[0,0,0,0]},{"id":3,"op":"param","pos":[40,220],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Aspect","text":"Aspect","index":1,"min":0.100000001,"max":8.0},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.300000012,0.0,0.0,0.0],"name":"Radius","text":"Radius","index":1,"min":0.00100000005,"max":2.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.0179999992,0.0,0.0,0.0],"name":"Thickness","text":"Thickness","index":1,"min":0.00100000005,"max":0.5},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.00400000019,0.0,0.0,0.0],"name":"Softness","text":"Softness","index":1,"min":9.99999975e-05,"max":0.100000001},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"MidAngle","text":"Mid Angle","index":1,"min":-6.28299999,"max":6.28299999},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[3.4000001,0.0,0.0,0.0],"name":"ArcAngle","text":"Arc Angle","index":1,"min":0.00999999978,"max":6.28299999},{"id":9,"op":"param","pos":[40,880],"in":[],"value":[0.00999999978,0.0,0.0,0.0],"name":"AngularSoftness","text":"Angular Softness","index":1,"min":9.99999975e-05,"max":0.5},{"id":10,"op":"param","pos":[40,990],"in":[],"value":[0.0799999982,0.0,0.0,0.0],"name":"GlowRadius","text":"Glow Radius","index":1,"min":0.00100000005,"max":1.0},{"id":11,"op":"param","pos":[40,1100],"in":[],"value":[2.0,0.0,0.0,0.0],"name":"GlowFalloff","text":"Glow Falloff","index":1,"min":0.100000001,"max":16.0},{"id":12,"op":"param","pos":[40,1210],"in":[],"value":[0.699999988,0.0,0.0,0.0],"name":"GlowIntensity","text":"Glow Intensity","index":1,"min":0.0,"max":16.0},{"id":13,"op":"param","pos":[40,1320],"in":[],"value":[2.0,0.0,0.0,0.0],"name":"Intensity","text":"Intensity","index":1,"min":0.0,"max":64.0},{"id":14,"op":"param","pos":[40,1430],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Opacity","text":"Opacity","index":1},{"id":15,"op":"param2","pos":[40,1540],"in":[],"value":[0.5,0.5,0.0,0.0],"name":"Center","text":"Center","index":1},{"id":16,"op":"paramcolor","pos":[40,1650],"in":[],"value":[0.100000001,1.10000002,2.5,1.0],"name":"Color","text":"Color","index":1},{"id":17,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0],[11,0],[12,0],[13,0],[14,0],[15,0],[16,0]],"value":[0,0,0,0],"name":"SDF Arc","text":"    float2 p=UV-Center.xy; p.x*=Aspect; float angle=atan2(p.y,p.x); float radius=length(p); float da=abs(atan2(sin(angle-MidAngle),cos(angle-MidAngle))); float arcMask=1.0-smoothstep(ArcAngle*0.5,ArcAngle*0.5+AngularSoftness,da); float ring=1.0-smoothstep(Thickness,Thickness+Softness,abs(radius-Radius)); float capAngle=ArcAngle*0.5; float2 c0=float2(cos(MidAngle-capAngle),sin(MidAngle-capAngle))*Radius; float2 c1=float2(cos(MidAngle+capAngle),sin(MidAngle+capAngle))*Radius; float caps=max(1.0-smoothstep(Thickness,Thickness+Softness,length(p-c0)),1.0-smoothstep(Thickness,Thickness+Softness,length(p-c1))); float m=max(ring*arcMask,caps); float glow=pow(saturate(1.0-abs(radius-Radius)/max(GlowRadius,0.0001)),GlowFalloff)*arcMask; float3 rgb=Color.rgb*(m+glow*GlowIntensity)*Intensity; return float4(rgb,saturate(max(m,glow)*Opacity));","pins":[["UV",2],["CyCanvasSize",4],["Aspect",1],["Radius",1],["Thickness",1],["Softness",1],["MidAngle",1],["ArcAngle",1],["AngularSoftness",1],["GlowRadius",1],["GlowFalloff",1],["GlowIntensity",1],["Intensity",1],["Opacity",1],["Center",3],["Color",3]],"outs":[["Resultat",4]]},{"id":18,"op":"output","pos":[780,40],"in":[[17,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
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
