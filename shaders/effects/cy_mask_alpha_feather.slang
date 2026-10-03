//@name Alpha Feather
//@category Masques
//@graph {"name":"Alpha Feather","category":"Masques","inputs":1,"premultiplied":false,"nextId":9,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.5,0.0,0.0,0.0],"name":"Threshold","text":"Threshold","index":1},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.100000001,0.0,0.0,0.0],"name":"Feather","text":"Feather","index":1,"min":9.99999975e-05,"max":0.5},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Gamma","text":"Gamma","index":1,"min":0.00999999978,"max":8.0},{"id":7,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0]],"value":[0,0,0,0],"name":"Alpha Feather","text":"    float a=Texture2DSample(CyInput,CyInputSampler,UV).a; float v=smoothstep(Threshold-Feather,Threshold+Feather,a); v=pow(saturate(v),max(Gamma,0.01)); return float4(v.xxx,1);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Threshold",1],["Feather",1],["Gamma",1]],"outs":[["Resultat",4]]},{"id":8,"op":"output","pos":[780,40],"in":[[7,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo 0
//@param Threshold float 0.5 0 1 "Threshold"
//@param Feather float 0.1 0.0001 0.5 "Feather"
//@param Gamma float 1 0.01 8 "Gamma"

// Imported preset (Mask / Mask): Remaps and feathers the source alpha channel.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Threshold, float Feather, float Gamma)
{
    float a=Texture2DSample(CyInput,CyInputSampler,UV).a; float v=smoothstep(Threshold-Feather,Threshold+Feather,a); v=pow(saturate(v),max(Gamma,0.01)); return float4(v.xxx,1);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Threshold, Feather, Gamma);
}
