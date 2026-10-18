//@name Sharpen
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Sharpen","nextId":7,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"name":"Strength","op":"param","pos":[40, 330],"text":"Strength","value":[0.5, 0, 0, 0]},{"id":5,"in":[[1, 0],[2, 0],[3, 0],[4, 0]],"name":"Sharpen","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Strength",1]],"pos":[420, 40],"text":"    float2 t = float2(1.0 / max(CyCanvasSize.x, 1.0), 1.0 / max(CyCanvasSize.y, 1.0));\n    float4 c = Texture2DSample(CyInput, CyInputSampler, UV);\n    float4 n = Texture2DSample(CyInput, CyInputSampler, UV + float2(t.x, 0)) + Texture2DSample(CyInput, CyInputSampler, UV - float2(t.x, 0)) + Texture2DSample(CyInput, CyInputSampler, UV + float2(0, t.y)) + Texture2DSample(CyInput, CyInputSampler, UV - float2(0, t.y));\n    return lerp(c, c * 5.0 - n, saturate(Strength));","value":[0, 0, 0, 0]},{"id":6,"in":[[5, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo full
//@param Strength float 0.5 0 1 "Strength"
// Imported preset (Effect / Detail): Five-tap sharpen filter with live strength.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Strength)
{
    float2 t = float2(1.0 / max(CyCanvasSize.x, 1.0), 1.0 / max(CyCanvasSize.y, 1.0));
    float4 c = Texture2DSample(CyInput, CyInputSampler, UV);
    float4 n = Texture2DSample(CyInput, CyInputSampler, UV + float2(t.x, 0)) + Texture2DSample(CyInput, CyInputSampler, UV - float2(t.x, 0)) + Texture2DSample(CyInput, CyInputSampler, UV + float2(0, t.y)) + Texture2DSample(CyInput, CyInputSampler, UV - float2(0, t.y));
    return lerp(c, c * 5.0 - n, saturate(Strength));
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Strength);
}
