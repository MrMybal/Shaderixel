//@name White Balance
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"White Balance","nextId":9,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":1,"min":-1,"name":"Temperature","op":"param","pos":[40, 330],"text":"Temperature","value":[0, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":1,"min":-1,"name":"Tint","op":"param","pos":[40, 440],"text":"Tint","value":[0, 0, 0, 0]},{"id":6,"in":[],"index":1,"name":"Strength","op":"param","pos":[40, 550],"text":"Strength","value":[1, 0, 0, 0]},{"id":7,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0]],"name":"White Balance","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Temperature",1],["Tint",1],["Strength",1]],"pos":[420, 40],"text":"    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);\n    float3 balance = float3(1.0 + Temperature * 0.12 - Tint * 0.04, 1.0 + Tint * 0.08, 1.0 - Temperature * 0.12 - Tint * 0.04);\n    float3 v = max(s.rgb, 0.0) * max(balance, 0.001);\n    return float4(lerp(s.rgb, v, saturate(Strength)), s.a);","value":[0, 0, 0, 0]},{"id":8,"in":[[7, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo 0
//@param Temperature float 0 -1 1 "Temperature"
//@param Tint float 0 -1 1 "Tint"
//@param Strength float 1 0 1 "Strength"
// Imported preset (Effect / Color & Tone): HDR-safe temperature and tint correction with a neutral default.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Temperature, float Tint, float Strength)
{
    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);
    float3 balance = float3(1.0 + Temperature * 0.12 - Tint * 0.04, 1.0 + Tint * 0.08, 1.0 - Temperature * 0.12 - Tint * 0.04);
    float3 v = max(s.rgb, 0.0) * max(balance, 0.001);
    return float4(lerp(s.rgb, v, saturate(Strength)), s.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Temperature, Tint, Strength);
}
