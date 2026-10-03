//@name Threshold
//@category Masques
//@graph {"name":"Threshold","category":"Masques","inputs":1,"premultiplied":false,"nextId":9,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.5,0.0,0.0,0.0],"name":"Threshold","text":"Threshold","index":1},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.0500000007,0.0,0.0,0.0],"name":"Softness","text":"Softness","index":1,"min":0.0,"max":0.5},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Invert","text":"Invert","index":1},{"id":7,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0]],"value":[0,0,0,0],"name":"Threshold","text":"    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float l=dot(s.rgb,float3(0.2126,0.7152,0.0722)); float v=smoothstep(Threshold-Softness,Threshold+Softness,l); if(Invert>0.5) v=1.0-v; return float4(v.xxx,1);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Threshold",1],["Softness",1],["Invert",1]],"outs":[["Resultat",4]]},{"id":8,"op":"output","pos":[780,40],"in":[[7,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo 0
//@param Threshold float 0.5 0 1 "Threshold"
//@param Softness float 0.05 0 0.5 "Softness"
//@param Invert float 0 0 1 "Invert"

// Imported preset (Mask / Mask): Soft luminance threshold mask.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Threshold, float Softness, float Invert)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float l=dot(s.rgb,float3(0.2126,0.7152,0.0722)); float v=smoothstep(Threshold-Softness,Threshold+Softness,l); if(Invert>0.5) v=1.0-v; return float4(v.xxx,1);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Threshold, Softness, Invert);
}
