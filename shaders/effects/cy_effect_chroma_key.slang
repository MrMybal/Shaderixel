//@name Chroma Key
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Chroma Key","nextId":10,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":4,"min":0,"name":"Tolerance","op":"param","pos":[40, 330],"text":"Tolerance","value":[0.25, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":2,"min":9.99999975e-05,"name":"Softness","op":"param","pos":[40, 440],"text":"Softness","value":[0.150000006, 0, 0, 0]},{"id":6,"in":[],"index":1,"name":"Despill","op":"param","pos":[40, 550],"text":"Despill","value":[0.5, 0, 0, 0]},{"id":7,"in":[],"index":1,"name":"KeyColor","op":"paramcolor","pos":[40, 660],"text":"Key Color","value":[0, 1, 0, 1]},{"id":8,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0]],"name":"Chroma Key","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Tolerance",1],["Softness",1],["Despill",1],["KeyColor",3]],"pos":[420, 40],"text":"    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);\n    float3 n = s.rgb / max(dot(s.rgb, float3(.333333, .333333, .333333)), .001);\n    float3 k = KeyColor.rgb / max(dot(KeyColor.rgb, float3(.333333, .333333, .333333)), .001);\n    float d = distance(n, k);\n    float keep = smoothstep(Tolerance, Tolerance + max(Softness, .0001), d);\n    float spill = saturate(1.0 - keep) * Despill;\n    float3 v = lerp(s.rgb, dot(s.rgb, float3(.2126, .7152, .0722)).xxx, spill);\n    return float4(v, s.a * keep);","value":[0, 0, 0, 0]},{"id":9,"in":[[8, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo 0
//@param Tolerance float 0.25 0 4 "Tolerance"
//@param Softness float 0.15 0.0001 2 "Softness"
//@param Despill float 0.5 0 1 "Despill"
//@param KeyColor color 0 1 0 1 "Key Color"
// Imported preset (Effect / Color & Tone): Removes a selected key color with tolerance, softness, edge restoration, and spill suppression.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Tolerance, float Softness, float Despill, float3 KeyColor)
{
    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);
    float3 n = s.rgb / max(dot(s.rgb, float3(.333333, .333333, .333333)), .001);
    float3 k = KeyColor.rgb / max(dot(KeyColor.rgb, float3(.333333, .333333, .333333)), .001);
    float d = distance(n, k);
    float keep = smoothstep(Tolerance, Tolerance + max(Softness, .0001), d);
    float spill = saturate(1.0 - keep) * Despill;
    float3 v = lerp(s.rgb, dot(s.rgb, float3(.2126, .7152, .0722)).xxx, spill);
    return float4(v, s.a * keep);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Tolerance, Softness, Despill, KeyColor.rgb);
}
