//@name Color Lookup
//@category Traitements
//@graph {"category":"Traitements","inputs":2,"name":"Color Lookup","nextId":9,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"index":1,"name":"LookupTexture","op":"texture","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"uv","pos":[40, 260],"value":[0, 0, 0, 0]},{"id":4,"in":[],"op":"canvassize","pos":[40, 350],"value":[0, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":64,"min":2,"name":"LUTSize","op":"param","pos":[40, 440],"text":"LUT Size","value":[16, 0, 0, 0]},{"id":6,"in":[],"index":1,"name":"Strength","op":"param","pos":[40, 550],"text":"Strength","value":[1, 0, 0, 0]},{"id":7,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0]],"name":"Color Lookup","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["LookupTexture",5],["UV",2],["CyCanvasSize",4],["LUTSize",1],["Strength",1]],"pos":[420, 40],"text":"    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);\n    float n = max(LUTSize, 2.0);\n    float blue = saturate(s.b) * (n - 1.0);\n    float slice0 = floor(blue);\n    float slice1 = min(slice0 + 1.0, n - 1.0);\n    float grid = ceil(sqrt(n));\n    float2 cell0 = float2(fmod(slice0, grid), floor(slice0 / grid));\n    float2 cell1 = float2(fmod(slice1, grid), floor(slice1 / grid));\n    float2 local = (s.rg * (n - 1.0) + 0.5) / n;\n    float2 uv0 = (cell0 + local) / grid;\n    float2 uv1 = (cell1 + local) / grid;\n    float3 lut = lerp(Texture2DSample(LookupTexture, LookupTextureSampler, uv0).rgb, Texture2DSample(LookupTexture, LookupTextureSampler, uv1).rgb, frac(blue));\n    return float4(lerp(s.rgb, lut, saturate(Strength)), s.a);","value":[0, 0, 0, 0]},{"id":8,"in":[[7, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@input Lookup
//@halo full
//@param LUTSize float 16 2 64 "LUT Size"
//@param Strength float 1 0 1 "Strength"
// Imported preset (Effect / Color & Tone): Applies a standard square 2D color lookup table with configurable grid size.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
static const uint LookupTexture = 1;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float LUTSize, float Strength)
{
    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);
    float n = max(LUTSize, 2.0);
    float blue = saturate(s.b) * (n - 1.0);
    float slice0 = floor(blue);
    float slice1 = min(slice0 + 1.0, n - 1.0);
    float grid = ceil(sqrt(n));
    float2 cell0 = float2(fmod(slice0, grid), floor(slice0 / grid));
    float2 cell1 = float2(fmod(slice1, grid), floor(slice1 / grid));
    float2 local = (s.rg * (n - 1.0) + 0.5) / n;
    float2 uv0 = (cell0 + local) / grid;
    float2 uv1 = (cell1 + local) / grid;
    float3 lut = lerp(Texture2DSample(LookupTexture, LookupTextureSampler, uv0).rgb, Texture2DSample(LookupTexture, LookupTextureSampler, uv1).rgb, frac(blue));
    return float4(lerp(s.rgb, lut, saturate(Strength)), s.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), LUTSize, Strength);
}
