//@name White Balance
//@category Traitements
//@graph {"name":"White Balance","category":"Traitements","inputs":1,"premultiplied":false,"nextId":9,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Temperature","text":"Temperature","index":1,"min":-1.0,"max":1.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Tint","text":"Tint","index":1,"min":-1.0,"max":1.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Strength","text":"Strength","index":1},{"id":7,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0]],"value":[0,0,0,0],"name":"White Balance","text":"    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float3 balance=float3(1.0+Temperature*0.12-Tint*0.04,1.0+Tint*0.08,1.0-Temperature*0.12-Tint*0.04); float3 v=max(s.rgb,0.0)*max(balance,0.001); return float4(lerp(s.rgb,v,saturate(Strength)),s.a);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Temperature",1],["Tint",1],["Strength",1]],"outs":[["Resultat",4]]},{"id":8,"op":"output","pos":[780,40],"in":[[7,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo 0
//@param Temperature float 0 -1 1 "Temperature"
//@param Tint float 0 -1 1 "Tint"
//@param Strength float 1 0 1 "Strength"

// Imported preset (Effect / Color & Tone): HDR-safe temperature and tint correction with a neutral default.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Temperature, float Tint, float Strength)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float3 balance=float3(1.0+Temperature*0.12-Tint*0.04,1.0+Tint*0.08,1.0-Temperature*0.12-Tint*0.04); float3 v=max(s.rgb,0.0)*max(balance,0.001); return float4(lerp(s.rgb,v,saturate(Strength)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Temperature, Tint, Strength);
}
