//@name Gradient Map
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Gradient Map","nextId":11,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":0.99000001,"min":0.00999999978,"name":"Midpoint","op":"param","pos":[40, 330],"text":"Midpoint","value":[0.5, 0, 0, 0]},{"id":5,"in":[],"index":1,"name":"Strength","op":"param","pos":[40, 440],"text":"Strength","value":[1, 0, 0, 0]},{"id":6,"in":[],"index":1,"name":"ShadowColor","op":"paramcolor","pos":[40, 550],"text":"Shadow Color","value":[0, 0, 0, 1]},{"id":7,"in":[],"index":1,"name":"MidColor","op":"paramcolor","pos":[40, 660],"text":"Mid Color","value":[0.5, 0.5, 0.5, 1]},{"id":8,"in":[],"index":1,"name":"HighlightColor","op":"paramcolor","pos":[40, 770],"text":"Highlight Color","value":[1, 1, 1, 1]},{"id":9,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0]],"name":"Gradient Map","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Midpoint",1],["Strength",1],["ShadowColor",3],["MidColor",3],["HighlightColor",3]],"pos":[420, 40],"text":"    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);\n    float l = saturate(dot(s.rgb, float3(.2126, .7152, .0722)));\n    float m = saturate(Midpoint);\n    float3 low = lerp(ShadowColor.rgb, MidColor.rgb, saturate(l / max(m, 0.0001)));\n    float3 high = lerp(MidColor.rgb, HighlightColor.rgb, saturate((l - m) / max(1.0 - m, 0.0001)));\n    float3 mapped = lerp(low, high, step(m, l));\n    return float4(lerp(s.rgb, mapped, saturate(Strength)), s.a);","value":[0, 0, 0, 0]},{"id":10,"in":[[9, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo 0
//@param Midpoint float 0.5 0.01 0.99 "Midpoint"
//@param Strength float 1 0 1 "Strength"
//@param ShadowColor color 0 0 0 1 "Shadow Color"
//@param MidColor color 0.5 0.5 0.5 1 "Mid Color"
//@param HighlightColor color 1 1 1 1 "Highlight Color"
// Imported preset (Effect / Color & Tone): Maps luminance through shadow, midtone, and highlight HDR colors.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Midpoint, float Strength, float3 ShadowColor, float3 MidColor, float3 HighlightColor)
{
    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);
    float l = saturate(dot(s.rgb, float3(.2126, .7152, .0722)));
    float m = saturate(Midpoint);
    float3 low = lerp(ShadowColor.rgb, MidColor.rgb, saturate(l / max(m, 0.0001)));
    float3 high = lerp(MidColor.rgb, HighlightColor.rgb, saturate((l - m) / max(1.0 - m, 0.0001)));
    float3 mapped = lerp(low, high, step(m, l));
    return float4(lerp(s.rgb, mapped, saturate(Strength)), s.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Midpoint, Strength, ShadowColor.rgb, MidColor.rgb, HighlightColor.rgb);
}
