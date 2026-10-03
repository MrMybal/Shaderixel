//@name Fabric Weave
//@category Dessin
//@graph {"name":"Fabric Weave","category":"Dessin","inputs":0,"premultiplied":false,"nextId":13,"nodes":[{"id":1,"op":"uv","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"canvassize","pos":[40,130],"in":[],"value":[0,0,0,0]},{"id":3,"op":"param","pos":[40,220],"in":[],"value":[48.0,0.0,0.0,0.0],"name":"WarpThreads","text":"Warp Threads","index":1,"min":2.0,"max":512.0},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[48.0,0.0,0.0,0.0],"name":"WeftThreads","text":"Weft Threads","index":1,"min":2.0,"max":512.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[1.79999995,0.0,0.0,0.0],"name":"ThreadRoundness","text":"Thread Roundness","index":1,"min":0.100000001,"max":16.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.0799999982,0.0,0.0,0.0],"name":"Irregularity","text":"Irregularity","index":1,"min":0.0,"max":0.800000012},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Seed","text":"Seed","index":1,"min":-1000.0,"max":1000.0},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Opacity","text":"Opacity","index":1},{"id":9,"op":"paramcolor","pos":[40,880],"in":[],"value":[0.0799999982,0.180000007,0.340000004,1.0],"name":"WarpColor","text":"Warp Color","index":1},{"id":10,"op":"paramcolor","pos":[40,990],"in":[],"value":[0.550000012,0.620000005,0.699999988,1.0],"name":"WeftColor","text":"Weft Color","index":1},{"id":11,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0]],"value":[0,0,0,0],"name":"Fabric Weave","text":"    float2 q=UV*float2(max(WarpThreads,1.0),max(WeftThreads,1.0));float2 cell=floor(q);float2 p=frac(q)-.5;float over=fmod(cell.x+cell.y,2.0);float warp=pow(saturate(1.0-abs(p.x)*2.0),ThreadRoundness);float weft=pow(saturate(1.0-abs(p.y)*2.0),ThreadRoundness);float h=frac(sin(dot(cell+Seed,float2(127.1,311.7)))*43758.5453);warp*=lerp(1.0-Irregularity,1.0+Irregularity,h);weft*=lerp(1.0+Irregularity,1.0-Irregularity,h);float3 rgb=lerp(WeftColor.rgb,WarpColor.rgb,lerp(weft,warp,over));float shade=lerp(.55,1.2,max(warp,weft));return float4(rgb*shade,Opacity);","pins":[["UV",2],["CyCanvasSize",4],["WarpThreads",1],["WeftThreads",1],["ThreadRoundness",1],["Irregularity",1],["Seed",1],["Opacity",1],["WarpColor",3],["WeftColor",3]],"outs":[["Resultat",4]]},{"id":12,"op":"output","pos":[780,40],"in":[[11,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@generator
//@halo 0
//@param WarpThreads float 48 2 512 "Warp Threads"
//@param WeftThreads float 48 2 512 "Weft Threads"
//@param ThreadRoundness float 1.8 0.1 16 "Thread Roundness"
//@param Irregularity float 0.08 0 0.8 "Irregularity"
//@param Seed float 0 -1000 1000 "Seed"
//@param Opacity float 1 0 1 "Opacity"
//@param WarpColor color 0.08 0.18 0.34 1 "Warp Color"
//@param WeftColor color 0.55 0.62 0.7 1 "Weft Color"

// Imported preset (Draw / Surface): Procedural woven textile with over-under threads, fiber highlights, and weave irregularity.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float WarpThreads, float WeftThreads, float ThreadRoundness, float Irregularity, float Seed, float Opacity, float3 WarpColor, float3 WeftColor)
{
    float2 q=UV*float2(max(WarpThreads,1.0),max(WeftThreads,1.0));float2 cell=floor(q);float2 p=frac(q)-.5;float over=fmod(cell.x+cell.y,2.0);float warp=pow(saturate(1.0-abs(p.x)*2.0),ThreadRoundness);float weft=pow(saturate(1.0-abs(p.y)*2.0),ThreadRoundness);float h=frac(sin(dot(cell+Seed,float2(127.1,311.7)))*43758.5453);warp*=lerp(1.0-Irregularity,1.0+Irregularity,h);weft*=lerp(1.0+Irregularity,1.0-Irregularity,h);float3 rgb=lerp(WeftColor.rgb,WarpColor.rgb,lerp(weft,warp,over));float shade=lerp(.55,1.2,max(warp,weft));return float4(rgb*shade,Opacity);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), WarpThreads, WeftThreads, ThreadRoundness, Irregularity, Seed, Opacity, WarpColor.rgb, WeftColor.rgb);
}
