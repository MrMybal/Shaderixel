//@name Equalize
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Equalize","nextId":9,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":2,"min":0,"name":"Midpoint","op":"param","pos":[40, 330],"text":"Midpoint","value":[0.5, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":32,"min":0.00999999978,"name":"Contrast","op":"param","pos":[40, 440],"text":"Contrast","value":[6, 0, 0, 0]},{"id":6,"in":[],"index":1,"name":"Strength","op":"param","pos":[40, 550],"text":"Strength","value":[1, 0, 0, 0]},{"id":7,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0]],"name":"Equalize","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Midpoint",1],["Contrast",1],["Strength",1]],"pos":[420, 40],"text":"    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);\n    float l = dot(s.rgb, float3(.2126, .7152, .0722));\n    float mapped = 1.0 / (1.0 + exp(-(l - Midpoint) * Contrast));\n    float reference = 1.0 / (1.0 + exp(Midpoint * Contrast));\n    mapped = saturate((mapped - reference) / max(1.0 - reference, 0.0001));\n    float3 v = s.rgb * (mapped / max(l, 0.0001));\n    return float4(lerp(s.rgb, v, saturate(Strength)), s.a);","value":[0, 0, 0, 0]},{"id":8,"in":[[7, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo 0
//@param Midpoint float 0.5 0 2 "Midpoint"
//@param Contrast float 6 0.01 32 "Contrast"
//@param Strength float 1 0 1 "Strength"
// Imported preset (Effect / Color & Tone): Adaptive sigmoidal contrast equalization suitable for SDR and HDR sources.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Midpoint, float Contrast, float Strength)
{
    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);
    float l = dot(s.rgb, float3(.2126, .7152, .0722));
    float mapped = 1.0 / (1.0 + exp(-(l - Midpoint) * Contrast));
    float reference = 1.0 / (1.0 + exp(Midpoint * Contrast));
    mapped = saturate((mapped - reference) / max(1.0 - reference, 0.0001));
    float3 v = s.rgb * (mapped / max(l, 0.0001));
    return float4(lerp(s.rgb, v, saturate(Strength)), s.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Midpoint, Contrast, Strength);
}
