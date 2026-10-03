//@name Checker
//@category Dessin
//@graph {"name":"Checker","category":"Dessin","inputs":0,"premultiplied":false,"nextId":10,"nodes":[{"id":1,"op":"uv","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"canvassize","pos":[40,130],"in":[],"value":[0,0,0,0]},{"id":3,"op":"param","pos":[40,220],"in":[],"value":[8.0,0.0,0.0,0.0],"name":"Tiling","text":"Tiling","index":1,"min":1.0,"max":256.0},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"OpacityA","text":"Opacity A","index":1},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"OpacityB","text":"Opacity B","index":1},{"id":6,"op":"paramcolor","pos":[40,550],"in":[],"value":[0.0,0.0,0.0,1.0],"name":"ColorA","text":"Color A","index":1},{"id":7,"op":"paramcolor","pos":[40,660],"in":[],"value":[1.0,1.0,1.0,1.0],"name":"ColorB","text":"Color B","index":1},{"id":8,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0]],"value":[0,0,0,0],"name":"Checker","text":"    float2 cell=floor(UV*max(Tiling,1.0)); float v=fmod(cell.x+cell.y,2.0); return float4(lerp(ColorA,ColorB,v),lerp(OpacityA,OpacityB,v));","pins":[["UV",2],["CyCanvasSize",4],["Tiling",1],["OpacityA",1],["OpacityB",1],["ColorA",3],["ColorB",3]],"outs":[["Resultat",4]]},{"id":9,"op":"output","pos":[780,40],"in":[[8,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@generator
//@halo 0
//@param Tiling float 8 1 256 "Tiling"
//@param OpacityA float 1 0 1 "Opacity A"
//@param OpacityB float 1 0 1 "Opacity B"
//@param ColorA color 0 0 0 1 "Color A"
//@param ColorB color 1 1 1 1 "Color B"

// Imported preset (Draw / Shape & Gradient): Procedural checker pattern with independent colors.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Tiling, float OpacityA, float OpacityB, float3 ColorA, float3 ColorB)
{
    float2 cell=floor(UV*max(Tiling,1.0)); float v=fmod(cell.x+cell.y,2.0); return float4(lerp(ColorA,ColorB,v),lerp(OpacityA,OpacityB,v));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Tiling, OpacityA, OpacityB, ColorA.rgb, ColorB.rgb);
}
