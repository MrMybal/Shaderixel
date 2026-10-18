//@name Shadows Highlights
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Shadows Highlights","nextId":10,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":2,"min":0,"name":"ShadowAmount","op":"param","pos":[40, 330],"text":"Shadow Amount","value":[0.25, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":2,"min":0,"name":"HighlightAmount","op":"param","pos":[40, 440],"text":"Highlight Amount","value":[0.25, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":8,"min":0.00999999978,"name":"Radius","op":"param","pos":[40, 550],"text":"Radius","value":[2, 0, 0, 0]},{"id":7,"in":[],"index":1,"name":"Strength","op":"param","pos":[40, 660],"text":"Strength","value":[1, 0, 0, 0]},{"id":8,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0]],"name":"Shadows Highlights","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["ShadowAmount",1],["HighlightAmount",1],["Radius",1],["Strength",1]],"pos":[420, 40],"text":"    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);\n    float l = max(dot(s.rgb, float3(.2126, .7152, .0722)), 0.0);\n    float shadow = pow(saturate(1.0 - l), max(Radius, 0.01)) * ShadowAmount;\n    float highlight = pow(saturate(l), max(Radius, 0.01)) * HighlightAmount;\n    float3 v = s.rgb * (1.0 + shadow) / (1.0 + highlight * max(l, 0.0));\n    return float4(lerp(s.rgb, v, saturate(Strength)), s.a);","value":[0, 0, 0, 0]},{"id":9,"in":[[8, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo 0
//@param ShadowAmount float 0.25 0 2 "Shadow Amount"
//@param HighlightAmount float 0.25 0 2 "Highlight Amount"
//@param Radius float 2 0.01 8 "Radius"
//@param Strength float 1 0 1 "Strength"
// Imported preset (Effect / Color & Tone): Recovers shadows and compresses highlights while retaining HDR detail.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float ShadowAmount, float HighlightAmount, float Radius, float Strength)
{
    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);
    float l = max(dot(s.rgb, float3(.2126, .7152, .0722)), 0.0);
    float shadow = pow(saturate(1.0 - l), max(Radius, 0.01)) * ShadowAmount;
    float highlight = pow(saturate(l), max(Radius, 0.01)) * HighlightAmount;
    float3 v = s.rgb * (1.0 + shadow) / (1.0 + highlight * max(l, 0.0));
    return float4(lerp(s.rgb, v, saturate(Strength)), s.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), ShadowAmount, HighlightAmount, Radius, Strength);
}
