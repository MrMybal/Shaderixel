//@name Chroma Key
//@category Traitements
//@graph {"name":"Chroma Key","category":"Traitements","inputs":1,"premultiplied":false,"nextId":10,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.25,0.0,0.0,0.0],"name":"Tolerance","text":"Tolerance","index":1,"min":0.0,"max":4.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.150000006,0.0,0.0,0.0],"name":"Softness","text":"Softness","index":1,"min":9.99999975e-05,"max":2.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.5,0.0,0.0,0.0],"name":"Despill","text":"Despill","index":1},{"id":7,"op":"paramcolor","pos":[40,660],"in":[],"value":[0.0,1.0,0.0,1.0],"name":"KeyColor","text":"Key Color","index":1},{"id":8,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0]],"value":[0,0,0,0],"name":"Chroma Key","text":"    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float3 n=s.rgb/max(dot(s.rgb,float3(.333333,.333333,.333333)),.001); float3 k=KeyColor.rgb/max(dot(KeyColor.rgb,float3(.333333,.333333,.333333)),.001); float d=distance(n,k); float keep=smoothstep(Tolerance,Tolerance+max(Softness,.0001),d); float spill=saturate(1.0-keep)*Despill; float3 v=lerp(s.rgb,dot(s.rgb,float3(.2126,.7152,.0722)).xxx,spill); return float4(v,s.a*keep);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Tolerance",1],["Softness",1],["Despill",1],["KeyColor",3]],"outs":[["Resultat",4]]},{"id":9,"op":"output","pos":[780,40],"in":[[8,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo 0
//@param Tolerance float 0.25 0 4 "Tolerance"
//@param Softness float 0.15 0.0001 2 "Softness"
//@param Despill float 0.5 0 1 "Despill"
//@param KeyColor color 0 1 0 1 "Key Color"

// Imported preset (Effect / Color & Tone): Removes a selected key color with tolerance, softness, edge restoration, and spill suppression.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Tolerance, float Softness, float Despill, float3 KeyColor)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float3 n=s.rgb/max(dot(s.rgb,float3(.333333,.333333,.333333)),.001); float3 k=KeyColor.rgb/max(dot(KeyColor.rgb,float3(.333333,.333333,.333333)),.001); float d=distance(n,k); float keep=smoothstep(Tolerance,Tolerance+max(Softness,.0001),d); float spill=saturate(1.0-keep)*Despill; float3 v=lerp(s.rgb,dot(s.rgb,float3(.2126,.7152,.0722)).xxx,spill); return float4(v,s.a*keep);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Tolerance, Softness, Despill, KeyColor.rgb);
}
