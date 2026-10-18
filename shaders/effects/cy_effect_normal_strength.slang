//@name Normal Strength
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Normal Strength","nextId":8,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":8,"min":0,"name":"Strength","op":"param","pos":[40, 330],"text":"Strength","value":[1, 0, 0, 0]},{"id":5,"in":[],"index":1,"name":"FlipGreen","op":"param","pos":[40, 440],"text":"Flip Green","value":[0, 0, 0, 0]},{"id":6,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0]],"name":"Normal Strength","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Strength",1],["FlipGreen",1]],"pos":[420, 40],"text":"    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);\n    float3 n = s.xyz * 2.0 - 1.0;\n    n.xy *= Strength;\n    n.y *= lerp(1.0, -1.0, FlipGreen);\n    n.z = sqrt(saturate(1.0 - dot(n.xy, n.xy)));\n    n = normalize(n);\n    return float4(n * .5 + .5, s.a);","value":[0, 0, 0, 0]},{"id":7,"in":[[6, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo 0
//@param Strength float 1 0 8 "Strength"
//@param FlipGreen float 0 0 1 "Flip Green"
// Imported preset (Effect / PBR): Adjusts tangent-space normal strength while reconstructing a valid positive Z component.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Strength, float FlipGreen)
{
    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);
    float3 n = s.xyz * 2.0 - 1.0;
    n.xy *= Strength;
    n.y *= lerp(1.0, -1.0, FlipGreen);
    n.z = sqrt(saturate(1.0 - dot(n.xy, n.xy)));
    n = normalize(n);
    return float4(n * .5 + .5, s.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Strength, FlipGreen);
}
