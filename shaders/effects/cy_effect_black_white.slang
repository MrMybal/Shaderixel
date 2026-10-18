//@name Black White
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Black White","nextId":10,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"name":"TintStrength","op":"param","pos":[40, 330],"text":"Tint Strength","value":[0, 0, 0, 0]},{"id":5,"in":[],"index":1,"name":"Strength","op":"param","pos":[40, 440],"text":"Strength","value":[1, 0, 0, 0]},{"id":6,"in":[],"index":1,"name":"ChannelWeights","op":"paramcolor","pos":[40, 550],"text":"Channel Weights","value":[0.300000012, 0.589999974, 0.109999999, 0]},{"id":7,"in":[],"index":1,"name":"Tint","op":"paramcolor","pos":[40, 660],"text":"Tint","value":[0.75, 0.620000005, 0.419999987, 1]},{"id":8,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0]],"name":"Black White","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["TintStrength",1],["Strength",1],["ChannelWeights",3],["Tint",3]],"pos":[420, 40],"text":"    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);\n    float l = dot(s.rgb, ChannelWeights.rgb) / max(dot(ChannelWeights.rgb, 1.0), 0.0001);\n    float3 bw = lerp(l.xxx, l * Tint.rgb, saturate(TintStrength));\n    return float4(lerp(s.rgb, bw, saturate(Strength)), s.a);","value":[0, 0, 0, 0]},{"id":9,"in":[[8, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo 0
//@param TintStrength float 0 0 1 "Tint Strength"
//@param Strength float 1 0 1 "Strength"
//@param ChannelWeights color 0.3 0.59 0.11 0 "Channel Weights"
//@param Tint color 0.75 0.62 0.42 1 "Tint"
// Imported preset (Effect / Color & Tone): Photoshop-style black-and-white conversion with independent source-channel weights and tint.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float TintStrength, float Strength, float3 ChannelWeights, float3 Tint)
{
    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);
    float l = dot(s.rgb, ChannelWeights.rgb) / max(dot(ChannelWeights.rgb, 1.0), 0.0001);
    float3 bw = lerp(l.xxx, l * Tint.rgb, saturate(TintStrength));
    return float4(lerp(s.rgb, bw, saturate(Strength)), s.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), TintStrength, Strength, ChannelWeights.rgb, Tint.rgb);
}
