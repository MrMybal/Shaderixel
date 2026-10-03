//@name Levels
//@category Traitements
//@graph {"name":"Levels","category":"Traitements","inputs":1,"premultiplied":false,"nextId":9,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"BlackPoint","text":"Black Point","index":1},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"WhitePoint","text":"White Point","index":1},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Gamma","text":"Gamma","index":1,"min":0.0500000007,"max":8.0},{"id":7,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0]],"value":[0,0,0,0],"name":"Levels","text":"    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float3 v=saturate((s.rgb-BlackPoint)/max(WhitePoint-BlackPoint,0.0001)); v=pow(v,1.0/max(Gamma,0.0001)); return float4(v,s.a);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["BlackPoint",1],["WhitePoint",1],["Gamma",1]],"outs":[["Resultat",4]]},{"id":8,"op":"output","pos":[780,40],"in":[[7,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo 0
//@param BlackPoint float 0 0 1 "Black Point"
//@param WhitePoint float 1 0 1 "White Point"
//@param Gamma float 1 0.05 8 "Gamma"

// Imported preset (Effect / Color & Tone): Black point, white point, and gamma correction.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float BlackPoint, float WhitePoint, float Gamma)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float3 v=saturate((s.rgb-BlackPoint)/max(WhitePoint-BlackPoint,0.0001)); v=pow(v,1.0/max(Gamma,0.0001)); return float4(v,s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), BlackPoint, WhitePoint, Gamma);
}
