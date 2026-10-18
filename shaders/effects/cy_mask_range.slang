//@name Range
//@category Masques
//@graph {"category":"Masques","inputs":1,"name":"Range","nextId":9,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"name":"Minimum","op":"param","pos":[40, 330],"text":"Minimum","value":[0.25, 0, 0, 0]},{"id":5,"in":[],"index":1,"name":"Maximum","op":"param","pos":[40, 440],"text":"Maximum","value":[0.75, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":0.5,"min":9.99999975e-05,"name":"Softness","op":"param","pos":[40, 550],"text":"Softness","value":[0.0500000007, 0, 0, 0]},{"id":7,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0]],"name":"Range","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Minimum",1],["Maximum",1],["Softness",1]],"pos":[420, 40],"text":"    float3 c = Texture2DSample(CyInput, CyInputSampler, UV).rgb;\n    float l = dot(c, float3(0.2126, 0.7152, 0.0722));\n    float v = smoothstep(Minimum - Softness, Minimum + Softness, l) * (1.0 - smoothstep(Maximum - Softness, Maximum + Softness, l));\n    return float4(v.xxx, 1);","value":[0, 0, 0, 0]},{"id":8,"in":[[7, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo 0
//@param Minimum float 0.25 0 1 "Minimum"
//@param Maximum float 0.75 0 1 "Maximum"
//@param Softness float 0.05 0.0001 0.5 "Softness"
// Imported preset (Mask / Mask): Selects a soft luminance range.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Minimum, float Maximum, float Softness)
{
    float3 c = Texture2DSample(CyInput, CyInputSampler, UV).rgb;
    float l = dot(c, float3(0.2126, 0.7152, 0.0722));
    float v = smoothstep(Minimum - Softness, Minimum + Softness, l) * (1.0 - smoothstep(Maximum - Softness, Maximum + Softness, l));
    return float4(v.xxx, 1);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Minimum, Maximum, Softness);
}
