//@name Despill
//@category Traitements
//@graph {"name":"Despill","category":"Traitements","inputs":1,"premultiplied":false,"nextId":8,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[2.0,0.0,0.0,0.0],"name":"Amount","text":"Amount","index":1,"min":0.0,"max":16.0},{"id":5,"op":"paramcolor","pos":[40,440],"in":[],"value":[0.0,1.0,0.0,1.0],"name":"SpillColor","text":"Spill Color","index":1},{"id":6,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0]],"value":[0,0,0,0],"name":"Despill","text":"    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float l=dot(s.rgb,float3(.2126,.7152,.0722)); float dominance=max(dot(s.rgb,SpillColor.rgb)-max(s.r,s.b),0.0); float3 neutral=l.xxx; float3 v=lerp(s.rgb,neutral,saturate(dominance*Amount)); return float4(v,s.a);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Amount",1],["SpillColor",3]],"outs":[["Resultat",4]]},{"id":7,"op":"output","pos":[780,40],"in":[[6,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo 0
//@param Amount float 2 0 16 "Amount"
//@param SpillColor color 0 1 0 1 "Spill Color"

// Imported preset (Effect / Color & Tone): Reduces a selected spill color while preserving luminance.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Amount, float3 SpillColor)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float l=dot(s.rgb,float3(.2126,.7152,.0722)); float dominance=max(dot(s.rgb,SpillColor.rgb)-max(s.r,s.b),0.0); float3 neutral=l.xxx; float3 v=lerp(s.rgb,neutral,saturate(dominance*Amount)); return float4(v,s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Amount, SpillColor.rgb);
}
