//@name Normal From Height
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Normal From Height","nextId":8,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":16,"min":0.25,"name":"Radius","op":"param","pos":[40, 330],"text":"Radius","value":[1, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":64,"min":0.00999999978,"name":"Strength","op":"param","pos":[40, 440],"text":"Strength","value":[4, 0, 0, 0]},{"id":6,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0]],"name":"Normal From Height","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Radius",1],["Strength",1]],"pos":[420, 40],"text":"    float2 t = float2(1.0 / max(CyCanvasSize.x, 1.0), 1.0 / max(CyCanvasSize.y, 1.0)) * Radius;\n    float l = dot(Texture2DSample(CyInput, CyInputSampler, UV - float2(t.x, 0)).rgb, float3(.2126, .7152, .0722));\n    float r = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(t.x, 0)).rgb, float3(.2126, .7152, .0722));\n    float u = dot(Texture2DSample(CyInput, CyInputSampler, UV - float2(0, t.y)).rgb, float3(.2126, .7152, .0722));\n    float d = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(0, t.y)).rgb, float3(.2126, .7152, .0722));\n    float3 n = normalize(float3((l - r) * Strength, (u - d) * Strength, 1.0));\n    float a = Texture2DSample(CyInput, CyInputSampler, UV).a;\n    return float4(n * 0.5 + 0.5, a);","value":[0, 0, 0, 0]},{"id":7,"in":[[6, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo full
//@param Radius float 1 0.25 16 "Radius"
//@param Strength float 4 0.01 64 "Strength"
// Imported preset (Effect / Detail): Generates a tangent-space normal map from source luminance.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float Strength)
{
    float2 t = float2(1.0 / max(CyCanvasSize.x, 1.0), 1.0 / max(CyCanvasSize.y, 1.0)) * Radius;
    float l = dot(Texture2DSample(CyInput, CyInputSampler, UV - float2(t.x, 0)).rgb, float3(.2126, .7152, .0722));
    float r = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(t.x, 0)).rgb, float3(.2126, .7152, .0722));
    float u = dot(Texture2DSample(CyInput, CyInputSampler, UV - float2(0, t.y)).rgb, float3(.2126, .7152, .0722));
    float d = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(0, t.y)).rgb, float3(.2126, .7152, .0722));
    float3 n = normalize(float3((l - r) * Strength, (u - d) * Strength, 1.0));
    float a = Texture2DSample(CyInput, CyInputSampler, UV).a;
    return float4(n * 0.5 + 0.5, a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, Strength);
}
