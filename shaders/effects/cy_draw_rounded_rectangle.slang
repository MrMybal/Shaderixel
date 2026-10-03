//@name Rounded Rectangle
//@category Dessin
//@graph {"name":"Rounded Rectangle","category":"Dessin","inputs":0,"premultiplied":false,"nextId":13,"nodes":[{"id":1,"op":"uv","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"canvassize","pos":[40,130],"in":[],"value":[0,0,0,0]},{"id":3,"op":"param","pos":[40,220],"in":[],"value":[0.0500000007,0.0,0.0,0.0],"name":"CornerRadius","text":"Corner Radius","index":1,"min":0.0,"max":0.5},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.0199999996,0.0,0.0,0.0],"name":"BorderWidth","text":"Border Width","index":1,"min":0.00100000005,"max":0.5},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.00300000003,0.0,0.0,0.0],"name":"Softness","text":"Softness","index":1,"min":9.99999975e-05,"max":0.100000001},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Outline","text":"Outline","index":1},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Opacity","text":"Opacity","index":1},{"id":8,"op":"param2","pos":[40,770],"in":[],"value":[0.5,0.5,0.0,0.0],"name":"Center","text":"Center","index":1},{"id":9,"op":"param2","pos":[40,880],"in":[],"value":[0.300000012,0.200000003,0.0,0.0],"name":"HalfSize","text":"Half Size","index":1},{"id":10,"op":"paramcolor","pos":[40,990],"in":[],"value":[1.0,1.0,1.0,1.0],"name":"Color","text":"Color","index":1},{"id":11,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0]],"value":[0,0,0,0],"name":"Rounded Rectangle","text":"    float2 p=abs(UV-Center.xy)-HalfSize.xy+CornerRadius; float d=min(max(p.x,p.y),0.0)+length(max(p,0.0))-CornerRadius; float fill=1.0-smoothstep(0.0,Softness,d); float inside=1.0-smoothstep(-BorderWidth,-BorderWidth+Softness,d); float a=lerp(fill,saturate(fill-inside),saturate(Outline)); return float4(Color.rgb,a*Opacity);","pins":[["UV",2],["CyCanvasSize",4],["CornerRadius",1],["BorderWidth",1],["Softness",1],["Outline",1],["Opacity",1],["Center",3],["HalfSize",3],["Color",3]],"outs":[["Resultat",4]]},{"id":12,"op":"output","pos":[780,40],"in":[[11,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
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

// Imported preset (Draw / Shape & Gradient): Signed-distance rounded rectangle with border support.
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
