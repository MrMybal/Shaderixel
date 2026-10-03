//@name Posterize
//@category Traitements
//@graph {"name":"Posterize","category":"Traitements","inputs":1,"premultiplied":false,"nextId":7,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[8.0,0.0,0.0,0.0],"name":"Levels","text":"Levels","index":1,"min":2.0,"max":256.0},{"id":5,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0]],"value":[0,0,0,0],"name":"Posterize","text":"    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float n=max(Levels,2.0); return float4(floor(s.rgb*n)/max(n-1.0,1.0),s.a);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Levels",1]],"outs":[["Resultat",4]]},{"id":6,"op":"output","pos":[780,40],"in":[[5,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo 0
//@param Levels float 8 2 256 "Levels"

// Imported preset (Effect / Stylize): Quantizes RGB to a configurable number of levels.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Levels)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float n=max(Levels,2.0); return float4(floor(s.rgb*n)/max(n-1.0,1.0),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Levels);
}
