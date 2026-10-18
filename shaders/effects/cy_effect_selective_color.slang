//@name Selective Color
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Selective Color","nextId":11,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"name":"Tolerance","op":"param","pos":[40, 330],"text":"Tolerance","value":[0.200000003, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":1,"min":9.99999975e-05,"name":"Softness","op":"param","pos":[40, 440],"text":"Softness","value":[0.150000006, 0, 0, 0]},{"id":6,"in":[],"index":1,"name":"Strength","op":"param","pos":[40, 550],"text":"Strength","value":[1, 0, 0, 0]},{"id":7,"in":[],"index":1,"name":"TargetColor","op":"paramcolor","pos":[40, 660],"text":"Target Color","value":[1, 0, 0, 1]},{"id":8,"in":[],"index":1,"name":"Correction","op":"paramcolor","pos":[40, 770],"text":"Correction","value":[0, 0, 0, 0]},{"id":9,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0]],"name":"Selective Color","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Tolerance",1],["Softness",1],["Strength",1],["TargetColor",3],["Correction",3]],"pos":[420, 40],"text":"    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);\n    float d = length(saturate(s.rgb) - TargetColor.rgb);\n    float m = 1.0 - smoothstep(Tolerance, Tolerance + max(Softness, 0.0001), d);\n    float3 v = s.rgb + Correction.rgb * m;\n    return float4(lerp(s.rgb, v, saturate(Strength)), s.a);","value":[0, 0, 0, 0]},{"id":10,"in":[[9, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo 0
//@param Tolerance float 0.2 0 1 "Tolerance"
//@param Softness float 0.15 0.0001 1 "Softness"
//@param Strength float 1 0 1 "Strength"
//@param TargetColor color 1 0 0 1 "Target Color"
//@param Correction color 0 0 0 0 "Correction"
// Imported preset (Effect / Color & Tone): Selects a color range and applies an RGB correction with soft tolerance.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Tolerance, float Softness, float Strength, float3 TargetColor, float3 Correction)
{
    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);
    float d = length(saturate(s.rgb) - TargetColor.rgb);
    float m = 1.0 - smoothstep(Tolerance, Tolerance + max(Softness, 0.0001), d);
    float3 v = s.rgb + Correction.rgb * m;
    return float4(lerp(s.rgb, v, saturate(Strength)), s.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Tolerance, Softness, Strength, TargetColor.rgb, Correction.rgb);
}
