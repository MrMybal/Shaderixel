//@name Dither
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Dither","nextId":8,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":256,"min":2,"name":"Levels","op":"param","pos":[40, 330],"text":"Levels","value":[16, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":4,"min":0,"name":"Amount","op":"param","pos":[40, 440],"text":"Amount","value":[1, 0, 0, 0]},{"id":6,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0]],"name":"Dither","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Levels",1],["Amount",1]],"pos":[420, 40],"text":"    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);\n    float2 p = floor(UV * CyCanvasSize.xy);\n    float n = frac(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453) - 0.5;\n    float levels = max(Levels, 2.0);\n    float3 v = floor(saturate(s.rgb + n * Amount / levels) * levels) / max(levels - 1.0, 1.0);\n    return float4(v, s.a);","value":[0, 0, 0, 0]},{"id":7,"in":[[6, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo 0
//@param Levels float 16 2 256 "Levels"
//@param Amount float 1 0 4 "Amount"
// Imported preset (Effect / Stylize): Ordered-style procedural dithering and bit-depth reduction.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Levels, float Amount)
{
    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);
    float2 p = floor(UV * CyCanvasSize.xy);
    float n = frac(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453) - 0.5;
    float levels = max(Levels, 2.0);
    float3 v = floor(saturate(s.rgb + n * Amount / levels) * levels) / max(levels - 1.0, 1.0);
    return float4(v, s.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Levels, Amount);
}
