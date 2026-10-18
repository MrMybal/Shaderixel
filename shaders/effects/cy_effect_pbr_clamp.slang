//@name PBR Clamp
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"PBR Clamp","nextId":12,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"name":"AlphaMinimum","op":"param","pos":[40, 330],"text":"Alpha Minimum","value":[0, 0, 0, 0]},{"id":5,"in":[],"index":1,"name":"AlphaMaximum","op":"param","pos":[40, 440],"text":"Alpha Maximum","value":[1, 0, 0, 0]},{"id":6,"in":[],"index":1,"name":"ClampAlpha","op":"param","pos":[40, 550],"text":"Clamp Alpha","value":[0, 0, 0, 0]},{"id":7,"in":[],"index":1,"name":"Minimum","op":"paramcolor","pos":[40, 660],"text":"Minimum","value":[0, 0, 0, 0]},{"id":8,"in":[],"index":1,"name":"Maximum","op":"paramcolor","pos":[40, 770],"text":"Maximum","value":[1, 1, 1, 0]},{"id":9,"in":[],"index":1,"name":"Channels","op":"paramcolor","pos":[40, 880],"text":"Channels","value":[1, 1, 1, 0]},{"id":10,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0]],"name":"PBR Clamp","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["AlphaMinimum",1],["AlphaMaximum",1],["ClampAlpha",1],["Minimum",3],["Maximum",3],["Channels",3]],"pos":[420, 40],"text":"    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);\n    float3 clamped = clamp(s.rgb, Minimum.rgb, Maximum.rgb);\n    float3 rgb = lerp(s.rgb, clamped, Channels.rgb);\n    float alpha = lerp(s.a, clamp(s.a, AlphaMinimum, AlphaMaximum), ClampAlpha);\n    return float4(rgb, alpha);","value":[0, 0, 0, 0]},{"id":11,"in":[[10, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo 0
//@param AlphaMinimum float 0 0 1 "Alpha Minimum"
//@param AlphaMaximum float 1 0 1 "Alpha Maximum"
//@param ClampAlpha float 0 0 1 "Clamp Alpha"
//@param Minimum color 0 0 0 0 "Minimum"
//@param Maximum color 1 1 1 0 "Maximum"
//@param Channels color 1 1 1 0 "Channels"
// Imported preset (Effect / PBR): Clamps selected linear PBR channels to physically plausible per-channel ranges.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float AlphaMinimum, float AlphaMaximum, float ClampAlpha, float3 Minimum, float3 Maximum, float3 Channels)
{
    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);
    float3 clamped = clamp(s.rgb, Minimum.rgb, Maximum.rgb);
    float3 rgb = lerp(s.rgb, clamped, Channels.rgb);
    float alpha = lerp(s.a, clamp(s.a, AlphaMinimum, AlphaMaximum), ClampAlpha);
    return float4(rgb, alpha);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), AlphaMinimum, AlphaMaximum, ClampAlpha, Minimum.rgb, Maximum.rgb, Channels.rgb);
}
