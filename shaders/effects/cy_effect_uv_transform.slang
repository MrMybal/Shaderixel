//@name UV Transform
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"UV Transform","nextId":10,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":6.28299999,"min":-6.28299999,"name":"Rotation","op":"param","pos":[40, 330],"text":"Rotation","value":[0, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":16,"min":0.00999999978,"name":"Scale","op":"param","pos":[40, 440],"text":"Scale","value":[1, 0, 0, 0]},{"id":6,"in":[],"index":1,"name":"Pivot","op":"param2","pos":[40, 550],"text":"Pivot","value":[0.5, 0.5, 0, 0]},{"id":7,"in":[],"index":1,"name":"Offset","op":"param2","pos":[40, 660],"text":"Offset","value":[0, 0, 0, 0]},{"id":8,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0]],"name":"UV Transform","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Rotation",1],["Scale",1],["Pivot",3],["Offset",3]],"pos":[420, 40],"text":"    float2 p = UV - Pivot.xy - Offset.xy;\n    float c = cos(Rotation);\n    float s = sin(Rotation);\n    p = mul(float2x2(c, -s, s, c), p) / max(Scale, 0.0001);\n    float2 q = Pivot.xy + p;\n    return Texture2DSample(CyInput, CyInputSampler, q);","value":[0, 0, 0, 0]},{"id":9,"in":[[8, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo full
//@param Rotation float 0 -6.283 6.283 "Rotation"
//@param Scale float 1 0.01 16 "Scale"
//@param Pivot float2 0.5 0.5 0 1 "Pivot"
//@param Offset float2 0 0 0 1 "Offset"
// Imported preset (Effect / Distort): Full-screen translate, rotate, scale, and pivot transform.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Rotation, float Scale, float3 Pivot, float3 Offset)
{
    float2 p = UV - Pivot.xy - Offset.xy;
    float c = cos(Rotation);
    float s = sin(Rotation);
    p = mul(float2x2(c, -s, s, c), p) / max(Scale, 0.0001);
    float2 q = Pivot.xy + p;
    return Texture2DSample(CyInput, CyInputSampler, q);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Rotation, Scale, float3(Pivot, 0.0), float3(Offset, 0.0));
}
