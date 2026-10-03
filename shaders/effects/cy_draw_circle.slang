//@name Circle
//@category Dessin
//@graph {"name":"Circle","category":"Dessin","inputs":0,"premultiplied":false,"nextId":13,"nodes":[{"id":1,"op":"uv","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"canvassize","pos":[40,130],"in":[],"value":[0,0,0,0]},{"id":3,"op":"param","pos":[40,220],"in":[],"value":[0.300000012,0.0,0.0,0.0],"name":"Radius","text":"Radius","index":1,"min":0.00100000005,"max":2.0},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.0399999991,0.0,0.0,0.0],"name":"Thickness","text":"Thickness","index":1,"min":0.00100000005,"max":1.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.00499999989,0.0,0.0,0.0],"name":"Softness","text":"Softness","index":1,"min":9.99999975e-05,"max":0.25},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Aspect","text":"Aspect","index":1,"min":0.00999999978,"max":8.0},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Outline","text":"Outline","index":1},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Opacity","text":"Opacity","index":1},{"id":9,"op":"param2","pos":[40,880],"in":[],"value":[0.5,0.5,0.0,0.0],"name":"Center","text":"Center","index":1},{"id":10,"op":"paramcolor","pos":[40,990],"in":[],"value":[1.0,1.0,1.0,1.0],"name":"Color","text":"Color","index":1},{"id":11,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0]],"value":[0,0,0,0],"name":"Circle","text":"    float2 p=UV-Center.xy; p.x*=Aspect; float d=length(p); float outer=1.0-smoothstep(Radius,Radius+Softness,d); float inner=1.0-smoothstep(max(Radius-Thickness,0.0),max(Radius-Thickness,0.0)+Softness,d); float a=lerp(outer,saturate(outer-inner),saturate(Outline)); return float4(Color.rgb,a*Opacity);","pins":[["UV",2],["CyCanvasSize",4],["Radius",1],["Thickness",1],["Softness",1],["Aspect",1],["Outline",1],["Opacity",1],["Center",3],["Color",3]],"outs":[["Resultat",4]]},{"id":12,"op":"output","pos":[780,40],"in":[[11,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
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

// Imported preset (Draw / Shape & Gradient): Antialiased filled or outlined circle with HDR color.
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
