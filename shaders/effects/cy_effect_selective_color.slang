//@name Selective Color
//@category Traitements
//@graph {"name":"Selective Color","category":"Traitements","inputs":1,"premultiplied":false,"nextId":11,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.200000003,0.0,0.0,0.0],"name":"Tolerance","text":"Tolerance","index":1},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.150000006,0.0,0.0,0.0],"name":"Softness","text":"Softness","index":1,"min":9.99999975e-05,"max":1.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Strength","text":"Strength","index":1},{"id":7,"op":"paramcolor","pos":[40,660],"in":[],"value":[1.0,0.0,0.0,1.0],"name":"TargetColor","text":"Target Color","index":1},{"id":8,"op":"paramcolor","pos":[40,770],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Correction","text":"Correction","index":1},{"id":9,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0]],"value":[0,0,0,0],"name":"Selective Color","text":"    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float d=length(saturate(s.rgb)-TargetColor.rgb); float m=1.0-smoothstep(Tolerance,Tolerance+max(Softness,0.0001),d); float3 v=s.rgb+Correction.rgb*m; return float4(lerp(s.rgb,v,saturate(Strength)),s.a);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Tolerance",1],["Softness",1],["Strength",1],["TargetColor",3],["Correction",3]],"outs":[["Resultat",4]]},{"id":10,"op":"output","pos":[780,40],"in":[[9,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo 0
//@param Tolerance float 0.2 0 1 "Tolerance"
//@param Softness float 0.15 0.0001 1 "Softness"
//@param Strength float 1 0 1 "Strength"
//@param TargetColor color 1 0 0 1 "Target Color"
//@param Correction color 0 0 0 0 "Correction"

// Imported preset (Effect / Color & Tone): Selects a color range and applies an RGB correction with soft tolerance.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Tolerance, float Softness, float Strength, float3 TargetColor, float3 Correction)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float d=length(saturate(s.rgb)-TargetColor.rgb); float m=1.0-smoothstep(Tolerance,Tolerance+max(Softness,0.0001),d); float3 v=s.rgb+Correction.rgb*m; return float4(lerp(s.rgb,v,saturate(Strength)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Tolerance, Softness, Strength, TargetColor.rgb, Correction.rgb);
}
