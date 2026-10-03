//@name Alpha
//@category Masques
//@graph {"name":"Alpha","category":"Masques","inputs":1,"premultiplied":false,"nextId":6,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0]],"value":[0,0,0,0],"name":"Alpha","text":"    float v=Texture2DSample(CyInput,CyInputSampler,UV).a; return float4(v.xxx,1);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4]],"outs":[["Resultat",4]]},{"id":5,"op":"output","pos":[780,40],"in":[[4,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo 0

// Imported preset (Mask / Mask): Source alpha mask.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize)
{
    float v=Texture2DSample(CyInput,CyInputSampler,UV).a; return float4(v.xxx,1);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel));
}
