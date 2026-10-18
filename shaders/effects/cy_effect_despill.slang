//@name Despill
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Despill","nextId":8,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":16,"min":0,"name":"Amount","op":"param","pos":[40, 330],"text":"Amount","value":[2, 0, 0, 0]},{"id":5,"in":[],"index":1,"name":"SpillColor","op":"paramcolor","pos":[40, 440],"text":"Spill Color","value":[0, 1, 0, 1]},{"id":6,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0]],"name":"Despill","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Amount",1],["SpillColor",3]],"pos":[420, 40],"text":"    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);\n    float l = dot(s.rgb, float3(.2126, .7152, .0722));\n    float dominance = max(dot(s.rgb, SpillColor.rgb) - max(s.r, s.b), 0.0);\n    float3 neutral = l.xxx;\n    float3 v = lerp(s.rgb, neutral, saturate(dominance * Amount));\n    return float4(v, s.a);","value":[0, 0, 0, 0]},{"id":7,"in":[[6, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo 0
//@param Amount float 2 0 16 "Amount"
//@param SpillColor color 0 1 0 1 "Spill Color"
// Imported preset (Effect / Color & Tone): Reduces a selected spill color while preserving luminance.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Amount, float3 SpillColor)
{
    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);
    float l = dot(s.rgb, float3(.2126, .7152, .0722));
    float dominance = max(dot(s.rgb, SpillColor.rgb) - max(s.r, s.b), 0.0);
    float3 neutral = l.xxx;
    float3 v = lerp(s.rgb, neutral, saturate(dominance * Amount));
    return float4(v, s.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Amount, SpillColor.rgb);
}
