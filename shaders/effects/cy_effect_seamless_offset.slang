//@name Seamless Offset
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Seamless Offset","nextId":9,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":0.5,"min":0.00100000005,"name":"Feather","op":"param","pos":[40, 330],"text":"Feather","value":[0.0799999982, 0, 0, 0]},{"id":5,"in":[],"index":1,"name":"Strength","op":"param","pos":[40, 440],"text":"Strength","value":[1, 0, 0, 0]},{"id":6,"in":[],"index":1,"name":"Offset","op":"param2","pos":[40, 550],"text":"Offset","value":[0.5, 0.5, 0, 0]},{"id":7,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0]],"name":"Seamless Offset","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Feather",1],["Strength",1],["Offset",3]],"pos":[420, 40],"text":"    float2 uv = frac(UV + Offset.xy);\n    float4 base = Texture2DSample(CyInput, CyInputSampler, uv);\n    float2 edge = min(uv, 1.0 - uv);\n    float wx = 1.0 - smoothstep(0.0, Feather, edge.x);\n    float wy = 1.0 - smoothstep(0.0, Feather, edge.y);\n    float4 x = Texture2DSample(CyInput, CyInputSampler, frac(uv + float2(.5, 0)));\n    float4 y = Texture2DSample(CyInput, CyInputSampler, frac(uv + float2(0, .5)));\n    float4 xy = Texture2DSample(CyInput, CyInputSampler, frac(uv + .5));\n    float4 blended = lerp(lerp(base, x, wx), lerp(y, xy, wx), wy);\n    return lerp(base, blended, Strength);","value":[0, 0, 0, 0]},{"id":8,"in":[[7, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo full
//@param Feather float 0.08 0.001 0.5 "Feather"
//@param Strength float 1 0 1 "Strength"
//@param Offset float2 0.5 0.5 0 1 "Offset"
// Imported preset (Effect / PBR): Wraps and crossfades an offset texture to reduce visible borders during seamless texture authoring.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Feather, float Strength, float3 Offset)
{
    float2 uv = frac(UV + Offset.xy);
    float4 base = Texture2DSample(CyInput, CyInputSampler, uv);
    float2 edge = min(uv, 1.0 - uv);
    float wx = 1.0 - smoothstep(0.0, Feather, edge.x);
    float wy = 1.0 - smoothstep(0.0, Feather, edge.y);
    float4 x = Texture2DSample(CyInput, CyInputSampler, frac(uv + float2(.5, 0)));
    float4 y = Texture2DSample(CyInput, CyInputSampler, frac(uv + float2(0, .5)));
    float4 xy = Texture2DSample(CyInput, CyInputSampler, frac(uv + .5));
    float4 blended = lerp(lerp(base, x, wx), lerp(y, xy, wx), wy);
    return lerp(base, blended, Strength);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Feather, Strength, float3(Offset, 0.0));
}
