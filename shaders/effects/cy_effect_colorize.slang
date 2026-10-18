//@name Colorize
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Colorize","nextId":10,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":8,"min":0.00999999978,"name":"Gamma","op":"param","pos":[40, 330],"text":"Gamma","value":[1, 0, 0, 0]},{"id":5,"in":[],"index":1,"name":"Strength","op":"param","pos":[40, 440],"text":"Strength","value":[1, 0, 0, 0]},{"id":6,"in":[],"index":1,"name":"ShadowColor","op":"paramcolor","pos":[40, 550],"text":"Shadow Color","value":[0.0199999996, 0.0399999991, 0.119999997, 1]},{"id":7,"in":[],"index":1,"name":"HighlightColor","op":"paramcolor","pos":[40, 660],"text":"Highlight Color","value":[1, 0.349999994, 0.0799999982, 1]},{"id":8,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0]],"name":"Colorize","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Gamma",1],["Strength",1],["ShadowColor",3],["HighlightColor",3]],"pos":[420, 40],"text":"    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);\n    float l = saturate(dot(s.rgb, float3(.2126, .7152, .0722)));\n    float3 v = lerp(ShadowColor.rgb, HighlightColor.rgb, pow(l, max(Gamma, 0.01)));\n    return float4(lerp(s.rgb, v, saturate(Strength)), s.a);","value":[0, 0, 0, 0]},{"id":9,"in":[[8, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo 0
//@param Gamma float 1 0.01 8 "Gamma"
//@param Strength float 1 0 1 "Strength"
//@param ShadowColor color 0.02 0.04 0.12 1 "Shadow Color"
//@param HighlightColor color 1 0.35 0.08 1 "Highlight Color"
// Imported preset (Effect / Color & Tone): Maps luminance between two HDR colors.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Gamma, float Strength, float3 ShadowColor, float3 HighlightColor)
{
    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);
    float l = saturate(dot(s.rgb, float3(.2126, .7152, .0722)));
    float3 v = lerp(ShadowColor.rgb, HighlightColor.rgb, pow(l, max(Gamma, 0.01)));
    return float4(lerp(s.rgb, v, saturate(Strength)), s.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Gamma, Strength, ShadowColor.rgb, HighlightColor.rgb);
}
