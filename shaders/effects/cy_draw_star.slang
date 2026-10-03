//@name Star
//@category Dessin
//@graph {"name":"Star","category":"Dessin","inputs":0,"premultiplied":false,"nextId":15,"nodes":[{"id":1,"op":"uv","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"canvassize","pos":[40,130],"in":[],"value":[0,0,0,0]},{"id":3,"op":"param","pos":[40,220],"in":[],"value":[5.0,0.0,0.0,0.0],"name":"Points","text":"Points","index":1,"min":2.0,"max":64.0},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.119999997,0.0,0.0,0.0],"name":"InnerRadius","text":"Inner Radius","index":1,"min":0.00100000005,"max":2.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.319999993,0.0,0.0,0.0],"name":"OuterRadius","text":"Outer Radius","index":1,"min":0.00100000005,"max":2.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"PointSharpness","text":"Point Sharpness","index":1,"min":0.0500000007,"max":8.0},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Rotation","text":"Rotation","index":1,"min":-6.28299999,"max":6.28299999},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Aspect","text":"Aspect","index":1,"min":0.00999999978,"max":8.0},{"id":9,"op":"param","pos":[40,880],"in":[],"value":[0.00400000019,0.0,0.0,0.0],"name":"Softness","text":"Softness","index":1,"min":9.99999975e-05,"max":0.100000001},{"id":10,"op":"param","pos":[40,990],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Opacity","text":"Opacity","index":1},{"id":11,"op":"param2","pos":[40,1100],"in":[],"value":[0.5,0.5,0.0,0.0],"name":"Center","text":"Center","index":1},{"id":12,"op":"paramcolor","pos":[40,1210],"in":[],"value":[1.0,1.0,1.0,1.0],"name":"Color","text":"Color","index":1},{"id":13,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0],[11,0],[12,0]],"value":[0,0,0,0],"name":"Star","text":"    float2 p=UV-Center.xy; p.x*=Aspect; float a=atan2(p.y,p.x)+Rotation; float count=max(Points,2.0); float wave=0.5+0.5*cos(a*count); float r=lerp(InnerRadius,OuterRadius,pow(wave,max(PointSharpness,0.05))); float mask=1.0-smoothstep(r,r+Softness,length(p)); return float4(Color.rgb,mask*Opacity);","pins":[["UV",2],["CyCanvasSize",4],["Points",1],["InnerRadius",1],["OuterRadius",1],["PointSharpness",1],["Rotation",1],["Aspect",1],["Softness",1],["Opacity",1],["Center",3],["Color",3]],"outs":[["Resultat",4]]},{"id":14,"op":"output","pos":[780,40],"in":[[13,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
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
