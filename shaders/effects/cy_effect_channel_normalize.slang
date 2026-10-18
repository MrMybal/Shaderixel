//@name Channel Normalize
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Channel Normalize","nextId":12,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"name":"InputMinimum","op":"paramcolor","pos":[40, 330],"text":"Input Minimum","value":[0, 0, 0, 0]},{"id":5,"in":[],"index":1,"name":"InputMaximum","op":"paramcolor","pos":[40, 440],"text":"Input Maximum","value":[1, 1, 1, 0]},{"id":6,"in":[],"index":1,"name":"Gamma","op":"paramcolor","pos":[40, 550],"text":"Gamma","value":[1, 1, 1, 0]},{"id":7,"in":[],"index":1,"name":"OutputMinimum","op":"paramcolor","pos":[40, 660],"text":"Output Minimum","value":[0, 0, 0, 0]},{"id":8,"in":[],"index":1,"name":"OutputMaximum","op":"paramcolor","pos":[40, 770],"text":"Output Maximum","value":[1, 1, 1, 0]},{"id":9,"in":[],"index":1,"name":"Channels","op":"paramcolor","pos":[40, 880],"text":"Channels","value":[1, 1, 1, 0]},{"id":10,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0]],"name":"Channel Normalize","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["InputMinimum",3],["InputMaximum",3],["Gamma",3],["OutputMinimum",3],["OutputMaximum",3],["Channels",3]],"pos":[420, 40],"text":"    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);\n    float3 v = saturate((s.rgb - InputMinimum.rgb) / max(InputMaximum.rgb - InputMinimum.rgb, .0001));\n    v = pow(v, max(Gamma.rgb, .01));\n    v = lerp(OutputMinimum.rgb, OutputMaximum.rgb, v);\n    return float4(lerp(s.rgb, v, Channels.rgb), s.a);","value":[0, 0, 0, 0]},{"id":11,"in":[[10, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo 0
//@param InputMinimum color 0 0 0 0 "Input Minimum"
//@param InputMaximum color 1 1 1 0 "Input Maximum"
//@param Gamma color 1 1 1 0 "Gamma"
//@param OutputMinimum color 0 0 0 0 "Output Minimum"
//@param OutputMaximum color 1 1 1 0 "Output Maximum"
//@param Channels color 1 1 1 0 "Channels"
// Imported preset (Effect / PBR): Normalizes and remaps independently selected packed PBR channels with per-channel gamma.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float3 InputMinimum, float3 InputMaximum, float3 Gamma, float3 OutputMinimum, float3 OutputMaximum, float3 Channels)
{
    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);
    float3 v = saturate((s.rgb - InputMinimum.rgb) / max(InputMaximum.rgb - InputMinimum.rgb, .0001));
    v = pow(v, max(Gamma.rgb, .01));
    v = lerp(OutputMinimum.rgb, OutputMaximum.rgb, v);
    return float4(lerp(s.rgb, v, Channels.rgb), s.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), InputMinimum.rgb, InputMaximum.rgb, Gamma.rgb, OutputMinimum.rgb, OutputMaximum.rgb, Channels.rgb);
}
