//@name Linear Gradient
//@category Dessin
//@graph {"name":"Linear Gradient","category":"Dessin","inputs":0,"premultiplied":false,"nextId":11,"nodes":[{"id":1,"op":"uv","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"canvassize","pos":[40,130],"in":[],"value":[0,0,0,0]},{"id":3,"op":"param","pos":[40,220],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Angle","text":"Angle","index":1,"min":-3.14159012,"max":3.14159012},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Width","text":"Width","index":1,"min":0.00999999978,"max":4.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"OpacityA","text":"Opacity A","index":1},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"OpacityB","text":"Opacity B","index":1},{"id":7,"op":"paramcolor","pos":[40,660],"in":[],"value":[0.0,0.0,0.0,1.0],"name":"ColorA","text":"Color A","index":1},{"id":8,"op":"paramcolor","pos":[40,770],"in":[],"value":[1.0,1.0,1.0,1.0],"name":"ColorB","text":"Color B","index":1},{"id":9,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0]],"value":[0,0,0,0],"name":"Linear Gradient","text":"    float2 p=UV-0.5; float c=cos(Angle); float s=sin(Angle); float t=saturate(dot(p,float2(c,s))/max(Width,0.0001)+0.5); return float4(lerp(ColorA,ColorB,t),lerp(OpacityA,OpacityB,t));","pins":[["UV",2],["CyCanvasSize",4],["Angle",1],["Width",1],["OpacityA",1],["OpacityB",1],["ColorA",3],["ColorB",3]],"outs":[["Resultat",4]]},{"id":10,"op":"output","pos":[780,40],"in":[[9,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@generator
//@halo 0
//@param Angle float 0 -3.14159 3.14159 "Angle"
//@param Width float 1 0.01 4 "Width"
//@param OpacityA float 1 0 1 "Opacity A"
//@param OpacityB float 1 0 1 "Opacity B"
//@param ColorA color 0 0 0 1 "Color A"
//@param ColorB color 1 1 1 1 "Color B"

// Imported preset (Draw / Shape & Gradient): Two-color directional linear gradient.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Angle, float Width, float OpacityA, float OpacityB, float3 ColorA, float3 ColorB)
{
    float2 p=UV-0.5; float c=cos(Angle); float s=sin(Angle); float t=saturate(dot(p,float2(c,s))/max(Width,0.0001)+0.5); return float4(lerp(ColorA,ColorB,t),lerp(OpacityA,OpacityB,t));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Angle, Width, OpacityA, OpacityB, ColorA.rgb, ColorB.rgb);
}
