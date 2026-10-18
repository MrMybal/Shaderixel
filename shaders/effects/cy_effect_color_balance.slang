//@name Color Balance
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Color Balance","nextId":11,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"name":"PreserveLuminosity","op":"param","pos":[40, 330],"text":"Preserve Luminosity","value":[1, 0, 0, 0]},{"id":5,"in":[],"index":1,"name":"Strength","op":"param","pos":[40, 440],"text":"Strength","value":[1, 0, 0, 0]},{"id":6,"in":[],"index":1,"name":"ShadowBalance","op":"paramcolor","pos":[40, 550],"text":"Shadow Balance","value":[0, 0, 0, 0]},{"id":7,"in":[],"index":1,"name":"MidtoneBalance","op":"paramcolor","pos":[40, 660],"text":"Midtone Balance","value":[0, 0, 0, 0]},{"id":8,"in":[],"index":1,"name":"HighlightBalance","op":"paramcolor","pos":[40, 770],"text":"Highlight Balance","value":[0, 0, 0, 0]},{"id":9,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0]],"name":"Color Balance","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["PreserveLuminosity",1],["Strength",1],["ShadowBalance",3],["MidtoneBalance",3],["HighlightBalance",3]],"pos":[420, 40],"text":"    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);\n    float l = dot(s.rgb, float3(.2126, .7152, .0722));\n    float sh = saturate((0.5 - l) * 2.0);\n    float hi = saturate((l - 0.5) * 2.0);\n    float mid = saturate(1.0 - sh - hi);\n    float3 v = s.rgb + ShadowBalance.rgb * sh + MidtoneBalance.rgb * mid + HighlightBalance.rgb * hi;\n    float vl = dot(v, float3(.2126, .7152, .0722));\n    v += PreserveLuminosity * (l - vl);\n    return float4(lerp(s.rgb, v, saturate(Strength)), s.a);","value":[0, 0, 0, 0]},{"id":10,"in":[[9, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo 0
//@param PreserveLuminosity float 1 0 1 "Preserve Luminosity"
//@param Strength float 1 0 1 "Strength"
//@param ShadowBalance color 0 0 0 0 "Shadow Balance"
//@param MidtoneBalance color 0 0 0 0 "Midtone Balance"
//@param HighlightBalance color 0 0 0 0 "Highlight Balance"
// Imported preset (Effect / Color & Tone): Independent shadow, midtone, and highlight color balance with luminosity preservation.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float PreserveLuminosity, float Strength, float3 ShadowBalance, float3 MidtoneBalance, float3 HighlightBalance)
{
    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);
    float l = dot(s.rgb, float3(.2126, .7152, .0722));
    float sh = saturate((0.5 - l) * 2.0);
    float hi = saturate((l - 0.5) * 2.0);
    float mid = saturate(1.0 - sh - hi);
    float3 v = s.rgb + ShadowBalance.rgb * sh + MidtoneBalance.rgb * mid + HighlightBalance.rgb * hi;
    float vl = dot(v, float3(.2126, .7152, .0722));
    v += PreserveLuminosity * (l - vl);
    return float4(lerp(s.rgb, v, saturate(Strength)), s.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), PreserveLuminosity, Strength, ShadowBalance.rgb, MidtoneBalance.rgb, HighlightBalance.rgb);
}
