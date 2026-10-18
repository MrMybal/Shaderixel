//@name Emboss
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Emboss","nextId":11,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":6.28299999,"min":-6.28299999,"name":"Angle","op":"param","pos":[40, 330],"text":"Angle","value":[0.785000026, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":32,"min":0.25,"name":"Radius","op":"param","pos":[40, 440],"text":"Radius","value":[1, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":16,"min":0,"name":"Strength","op":"param","pos":[40, 550],"text":"Strength","value":[2, 0, 0, 0]},{"id":7,"in":[],"index":1,"max":2,"min":-2,"name":"Bias","op":"param","pos":[40, 660],"text":"Bias","value":[0.5, 0, 0, 0]},{"id":8,"in":[],"index":1,"name":"Blend","op":"param","pos":[40, 770],"text":"Blend","value":[1, 0, 0, 0]},{"id":9,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0]],"name":"Emboss","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Angle",1],["Radius",1],["Strength",1],["Bias",1],["Blend",1]],"pos":[420, 40],"text":"    float2 t = float2(cos(Angle) / max(CyCanvasSize.x, 1.0), sin(Angle) / max(CyCanvasSize.y, 1.0)) * Radius;\n    float4 a = Texture2DSample(CyInput, CyInputSampler, UV - t);\n    float4 b = Texture2DSample(CyInput, CyInputSampler, UV + t);\n    float3 e = (a.rgb - b.rgb) * Strength + Bias;\n    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);\n    return float4(lerp(s.rgb, e, saturate(Blend)), s.a);","value":[0, 0, 0, 0]},{"id":10,"in":[[9, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo full
//@param Angle float 0.785 -6.283 6.283 "Angle"
//@param Radius float 1 0.25 32 "Radius"
//@param Strength float 2 0 16 "Strength"
//@param Bias float 0.5 -2 2 "Bias"
//@param Blend float 1 0 1 "Blend"
// Imported preset (Effect / Detail): Directional emboss filter with adjustable bias.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Angle, float Radius, float Strength, float Bias, float Blend)
{
    float2 t = float2(cos(Angle) / max(CyCanvasSize.x, 1.0), sin(Angle) / max(CyCanvasSize.y, 1.0)) * Radius;
    float4 a = Texture2DSample(CyInput, CyInputSampler, UV - t);
    float4 b = Texture2DSample(CyInput, CyInputSampler, UV + t);
    float3 e = (a.rgb - b.rgb) * Strength + Bias;
    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);
    return float4(lerp(s.rgb, e, saturate(Blend)), s.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Angle, Radius, Strength, Bias, Blend);
}
