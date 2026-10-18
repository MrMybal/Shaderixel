//@name Normalize Range
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Normalize Range","nextId":11,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":32,"min":-32,"name":"InputMinimum","op":"param","pos":[40, 330],"text":"Input Minimum","value":[0, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":32,"min":-32,"name":"InputMaximum","op":"param","pos":[40, 440],"text":"Input Maximum","value":[1, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":32,"min":-32,"name":"OutputMinimum","op":"param","pos":[40, 550],"text":"Output Minimum","value":[0, 0, 0, 0]},{"id":7,"in":[],"index":1,"max":32,"min":-32,"name":"OutputMaximum","op":"param","pos":[40, 660],"text":"Output Maximum","value":[1, 0, 0, 0]},{"id":8,"in":[],"index":1,"name":"Strength","op":"param","pos":[40, 770],"text":"Strength","value":[1, 0, 0, 0]},{"id":9,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0]],"name":"Normalize Range","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["InputMinimum",1],["InputMaximum",1],["OutputMinimum",1],["OutputMaximum",1],["Strength",1]],"pos":[420, 40],"text":"    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);\n    float3 v = (s.rgb - InputMinimum) / max(InputMaximum - InputMinimum, .0001);\n    v = lerp(OutputMinimum, OutputMaximum, v);\n    return float4(lerp(s.rgb, v, saturate(Strength)), s.a);","value":[0, 0, 0, 0]},{"id":10,"in":[[9, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo 0
//@param InputMinimum float 0 -32 32 "Input Minimum"
//@param InputMaximum float 1 -32 32 "Input Maximum"
//@param OutputMinimum float 0 -32 32 "Output Minimum"
//@param OutputMaximum float 1 -32 32 "Output Maximum"
//@param Strength float 1 0 1 "Strength"
// Imported preset (Effect / Color & Tone): Remaps a chosen input range to a configurable output range.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float InputMinimum, float InputMaximum, float OutputMinimum, float OutputMaximum, float Strength)
{
    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);
    float3 v = (s.rgb - InputMinimum) / max(InputMaximum - InputMinimum, .0001);
    v = lerp(OutputMinimum, OutputMaximum, v);
    return float4(lerp(s.rgb, v, saturate(Strength)), s.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), InputMinimum, InputMaximum, OutputMinimum, OutputMaximum, Strength);
}
