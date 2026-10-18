//@name Alpha Feather
//@category Masques
//@graph {"category":"Masques","inputs":1,"name":"Alpha Feather","nextId":9,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"name":"Threshold","op":"param","pos":[40, 330],"text":"Threshold","value":[0.5, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":0.5,"min":9.99999975e-05,"name":"Feather","op":"param","pos":[40, 440],"text":"Feather","value":[0.100000001, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":8,"min":0.00999999978,"name":"Gamma","op":"param","pos":[40, 550],"text":"Gamma","value":[1, 0, 0, 0]},{"id":7,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0]],"name":"Alpha Feather","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Threshold",1],["Feather",1],["Gamma",1]],"pos":[420, 40],"text":"    float a = Texture2DSample(CyInput, CyInputSampler, UV).a;\n    float v = smoothstep(Threshold - Feather, Threshold + Feather, a);\n    v = pow(saturate(v), max(Gamma, 0.01));\n    return float4(v.xxx, 1);","value":[0, 0, 0, 0]},{"id":8,"in":[[7, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo 0
//@param Threshold float 0.5 0 1 "Threshold"
//@param Feather float 0.1 0.0001 0.5 "Feather"
//@param Gamma float 1 0.01 8 "Gamma"
// Imported preset (Mask / Mask): Remaps and feathers the source alpha channel.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Threshold, float Feather, float Gamma)
{
    float a = Texture2DSample(CyInput, CyInputSampler, UV).a;
    float v = smoothstep(Threshold - Feather, Threshold + Feather, a);
    v = pow(saturate(v), max(Gamma, 0.01));
    return float4(v.xxx, 1);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Threshold, Feather, Gamma);
}
