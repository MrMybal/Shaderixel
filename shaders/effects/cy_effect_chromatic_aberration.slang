//@name Chromatic Aberration
//@category Traitements
//@graph {"name":"Chromatic Aberration","category":"Traitements","inputs":1,"premultiplied":false,"nextId":8,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.0149999997,0.0,0.0,0.0],"name":"Amount","text":"Amount","index":1,"min":-0.25,"max":0.25},{"id":5,"op":"param2","pos":[40,440],"in":[],"value":[0.5,0.5,0.0,0.0],"name":"Center","text":"Center","index":1},{"id":6,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0]],"value":[0,0,0,0],"name":"Chromatic Aberration","text":"    float2 dir=UV-Center.xy; float2 o=dir*Amount; float r=Texture2DSample(CyInput,CyInputSampler,saturate(UV+o)).r; float4 g=Texture2DSample(CyInput,CyInputSampler,UV); float b=Texture2DSample(CyInput,CyInputSampler,saturate(UV-o)).b; return float4(r,g.g,b,g.a);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Amount",1],["Center",3]],"outs":[["Resultat",4]]},{"id":7,"op":"output","pos":[780,40],"in":[[6,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo full
//@param Amount float 0.015 -0.25 0.25 "Amount"
//@param Center float2 0.5 0.5 0 1 "Center"

// Imported preset (Effect / Color & Tone): Radial RGB channel separation for lens effects.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Amount, float3 Center)
{
    float2 dir=UV-Center.xy; float2 o=dir*Amount; float r=Texture2DSample(CyInput,CyInputSampler,saturate(UV+o)).r; float4 g=Texture2DSample(CyInput,CyInputSampler,UV); float b=Texture2DSample(CyInput,CyInputSampler,saturate(UV-o)).b; return float4(r,g.g,b,g.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Amount, float3(Center, 0.0));
}
