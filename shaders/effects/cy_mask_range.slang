//@name Range
//@category Masques
//@graph {"name":"Range","category":"Masques","inputs":1,"premultiplied":false,"nextId":9,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.25,0.0,0.0,0.0],"name":"Minimum","text":"Minimum","index":1},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.75,0.0,0.0,0.0],"name":"Maximum","text":"Maximum","index":1},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.0500000007,0.0,0.0,0.0],"name":"Softness","text":"Softness","index":1,"min":9.99999975e-05,"max":0.5},{"id":7,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0]],"value":[0,0,0,0],"name":"Range","text":"    float3 c=Texture2DSample(CyInput,CyInputSampler,UV).rgb; float l=dot(c,float3(0.2126,0.7152,0.0722)); float v=smoothstep(Minimum-Softness,Minimum+Softness,l)*(1.0-smoothstep(Maximum-Softness,Maximum+Softness,l)); return float4(v.xxx,1);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Minimum",1],["Maximum",1],["Softness",1]],"outs":[["Resultat",4]]},{"id":8,"op":"output","pos":[780,40],"in":[[7,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo 0
//@param Minimum float 0.25 0 1 "Minimum"
//@param Maximum float 0.75 0 1 "Maximum"
//@param Softness float 0.05 0.0001 0.5 "Softness"

// Imported preset (Mask / Mask): Selects a soft luminance range.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Minimum, float Maximum, float Softness)
{
    float3 c=Texture2DSample(CyInput,CyInputSampler,UV).rgb; float l=dot(c,float3(0.2126,0.7152,0.0722)); float v=smoothstep(Minimum-Softness,Minimum+Softness,l)*(1.0-smoothstep(Maximum-Softness,Maximum+Softness,l)); return float4(v.xxx,1);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Minimum, Maximum, Softness);
}
