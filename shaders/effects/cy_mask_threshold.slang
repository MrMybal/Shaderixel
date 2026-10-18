//@name Threshold
//@category Masques
//@graph {"category":"Masques","inputs":1,"name":"Threshold","nextId":9,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"name":"Threshold","op":"param","pos":[40, 330],"text":"Threshold","value":[0.5, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":0.5,"min":0,"name":"Softness","op":"param","pos":[40, 440],"text":"Softness","value":[0.0500000007, 0, 0, 0]},{"id":6,"in":[],"index":1,"name":"Invert","op":"param","pos":[40, 550],"text":"Invert","value":[0, 0, 0, 0]},{"id":7,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0]],"name":"Threshold","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Threshold",1],["Softness",1],["Invert",1]],"pos":[420, 40],"text":"    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);\n    float l = dot(s.rgb, float3(0.2126, 0.7152, 0.0722));\n    float v = smoothstep(Threshold - Softness, Threshold + Softness, l);\n    if (Invert > 0.5) v = 1.0 - v;\n    return float4(v.xxx, 1);","value":[0, 0, 0, 0]},{"id":8,"in":[[7, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo 0
//@param Threshold float 0.5 0 1 "Threshold"
//@param Softness float 0.05 0 0.5 "Softness"
//@param Invert float 0 0 1 "Invert"
// Imported preset (Mask / Mask): Soft luminance threshold mask.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Threshold, float Softness, float Invert)
{
    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);
    float l = dot(s.rgb, float3(0.2126, 0.7152, 0.0722));
    float v = smoothstep(Threshold - Softness, Threshold + Softness, l);
    if (Invert > 0.5) v = 1.0 - v;
    return float4(v.xxx, 1);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Threshold, Softness, Invert);
}
