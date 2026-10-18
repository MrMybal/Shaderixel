//@name Alpha Remap
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Alpha Remap","nextId":12,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"name":"Minimum","op":"param","pos":[40, 330],"text":"Minimum","value":[0, 0, 0, 0]},{"id":5,"in":[],"index":1,"name":"Maximum","op":"param","pos":[40, 440],"text":"Maximum","value":[1, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":0.5,"min":0,"name":"Feather","op":"param","pos":[40, 550],"text":"Feather","value":[0, 0, 0, 0]},{"id":7,"in":[],"index":1,"max":8,"min":0.00999999978,"name":"Gamma","op":"param","pos":[40, 660],"text":"Gamma","value":[1, 0, 0, 0]},{"id":8,"in":[],"index":1,"name":"Invert","op":"param","pos":[40, 770],"text":"Invert","value":[0, 0, 0, 0]},{"id":9,"in":[],"index":1,"name":"Strength","op":"param","pos":[40, 880],"text":"Strength","value":[1, 0, 0, 0]},{"id":10,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0]],"name":"Alpha Remap","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Minimum",1],["Maximum",1],["Feather",1],["Gamma",1],["Invert",1],["Strength",1]],"pos":[420, 40],"text":"    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);\n    float a = smoothstep(Minimum - Feather, Maximum + Feather, s.a);\n    a = pow(saturate(a), max(Gamma, 0.001));\n    a = lerp(a, 1.0 - a, saturate(Invert));\n    return float4(s.rgb, lerp(s.a, a, saturate(Strength)));","value":[0, 0, 0, 0]},{"id":11,"in":[[10, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo 0
//@param Minimum float 0 0 1 "Minimum"
//@param Maximum float 1 0 1 "Maximum"
//@param Feather float 0 0 0.5 "Feather"
//@param Gamma float 1 0.01 8 "Gamma"
//@param Invert float 0 0 1 "Invert"
//@param Strength float 1 0 1 "Strength"
// Imported preset (Effect / Color & Tone): Remaps, feathers, inverts, and gamma-corrects alpha without changing RGB.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Minimum, float Maximum, float Feather, float Gamma, float Invert, float Strength)
{
    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);
    float a = smoothstep(Minimum - Feather, Maximum + Feather, s.a);
    a = pow(saturate(a), max(Gamma, 0.001));
    a = lerp(a, 1.0 - a, saturate(Invert));
    return float4(s.rgb, lerp(s.a, a, saturate(Strength)));
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Minimum, Maximum, Feather, Gamma, Invert, Strength);
}
