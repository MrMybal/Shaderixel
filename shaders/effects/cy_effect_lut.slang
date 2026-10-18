//@name LUT
//@category Traitements
//@graph {"category":"Traitements","inputs":2,"name":"LUT","nextId":10,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"index":1,"name":"LUTTexture","op":"texture","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"uv","pos":[40, 260],"value":[0, 0, 0, 0]},{"id":4,"in":[],"op":"canvassize","pos":[40, 350],"value":[0, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":64,"min":2,"name":"LUTSize","op":"param","pos":[40, 440],"text":"LUT Size","value":[16, 0, 0, 0]},{"id":6,"in":[],"index":1,"name":"FlipVertical","op":"param","pos":[40, 550],"text":"Flip Vertical","value":[0, 0, 0, 0]},{"id":7,"in":[],"index":1,"name":"Strength","op":"param","pos":[40, 660],"text":"Strength","value":[0, 0, 0, 0]},{"id":8,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0]],"name":"LUT","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["LUTTexture",5],["UV",2],["CyCanvasSize",4],["LUTSize",1],["FlipVertical",1],["Strength",1]],"pos":[420, 40],"text":"    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);\n    float n = max(LUTSize, 2.0);\n    float3 c = saturate(s.rgb);\n    c.g = lerp(c.g, 1.0 - c.g, saturate(FlipVertical));\n    float z = c.b * (n - 1.0);\n    float z0 = floor(z);\n    float z1 = min(z0 + 1.0, n - 1.0);\n    float2 uv0 = float2((z0 * n + c.r * (n - 1.0) + 0.5) / (n * n), (c.g * (n - 1.0) + 0.5) / n);\n    float2 uv1 = float2((z1 * n + c.r * (n - 1.0) + 0.5) / (n * n), (c.g * (n - 1.0) + 0.5) / n);\n    float3 graded = lerp(Texture2DSample(LUTTexture, LUTTextureSampler, uv0).rgb, Texture2DSample(LUTTexture, LUTTextureSampler, uv1).rgb, frac(z));\n    return float4(lerp(s.rgb, graded, saturate(Strength)), s.a);","value":[0, 0, 0, 0]},{"id":9,"in":[[8, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@input LUT
//@halo full
//@param LUTSize float 16 2 64 "LUT Size"
//@param FlipVertical float 0 0 1 "Flip Vertical"
//@param Strength float 0 0 1 "Strength"
// Imported preset (Effect / Color & Tone): Applies a standard horizontal 2D strip LUT (N*N by N) with adjustable strength.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
static const uint LUTTexture = 1;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float LUTSize, float FlipVertical, float Strength)
{
    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);
    float n = max(LUTSize, 2.0);
    float3 c = saturate(s.rgb);
    c.g = lerp(c.g, 1.0 - c.g, saturate(FlipVertical));
    float z = c.b * (n - 1.0);
    float z0 = floor(z);
    float z1 = min(z0 + 1.0, n - 1.0);
    float2 uv0 = float2((z0 * n + c.r * (n - 1.0) + 0.5) / (n * n), (c.g * (n - 1.0) + 0.5) / n);
    float2 uv1 = float2((z1 * n + c.r * (n - 1.0) + 0.5) / (n * n), (c.g * (n - 1.0) + 0.5) / n);
    float3 graded = lerp(Texture2DSample(LUTTexture, LUTTextureSampler, uv0).rgb, Texture2DSample(LUTTexture, LUTTextureSampler, uv1).rgb, frac(z));
    return float4(lerp(s.rgb, graded, saturate(Strength)), s.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), LUTSize, FlipVertical, Strength);
}
