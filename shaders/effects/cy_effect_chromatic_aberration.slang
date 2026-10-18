//@name Chromatic Aberration
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Chromatic Aberration","nextId":8,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":0.25,"min":-0.25,"name":"Amount","op":"param","pos":[40, 330],"text":"Amount","value":[0.0149999997, 0, 0, 0]},{"id":5,"in":[],"index":1,"name":"Center","op":"param2","pos":[40, 440],"text":"Center","value":[0.5, 0.5, 0, 0]},{"id":6,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0]],"name":"Chromatic Aberration","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Amount",1],["Center",3]],"pos":[420, 40],"text":"    float2 dir = UV - Center.xy;\n    float2 o = dir * Amount;\n    float r = Texture2DSample(CyInput, CyInputSampler, saturate(UV + o)).r;\n    float4 g = Texture2DSample(CyInput, CyInputSampler, UV);\n    float b = Texture2DSample(CyInput, CyInputSampler, saturate(UV - o)).b;\n    return float4(r, g.g, b, g.a);","value":[0, 0, 0, 0]},{"id":7,"in":[[6, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo full
//@param Amount float 0.015 -0.25 0.25 "Amount"
//@param Center float2 0.5 0.5 0 1 "Center"
// Imported preset (Effect / Color & Tone): Radial RGB channel separation for lens effects.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Amount, float3 Center)
{
    float2 dir = UV - Center.xy;
    float2 o = dir * Amount;
    float r = Texture2DSample(CyInput, CyInputSampler, saturate(UV + o)).r;
    float4 g = Texture2DSample(CyInput, CyInputSampler, UV);
    float b = Texture2DSample(CyInput, CyInputSampler, saturate(UV - o)).b;
    return float4(r, g.g, b, g.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Amount, float3(Center, 0.0));
}
