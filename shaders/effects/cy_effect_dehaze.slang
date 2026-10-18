//@name Dehaze
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Dehaze","nextId":10,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":2,"min":-2,"name":"Amount","op":"param","pos":[40, 330],"text":"Amount","value":[0.5, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":1,"min":0.00999999978,"name":"MinimumTransmission","op":"param","pos":[40, 440],"text":"Minimum Transmission","value":[0.150000006, 0, 0, 0]},{"id":6,"in":[],"index":1,"name":"Strength","op":"param","pos":[40, 550],"text":"Strength","value":[1, 0, 0, 0]},{"id":7,"in":[],"index":1,"name":"HazeColor","op":"paramcolor","pos":[40, 660],"text":"Haze Color","value":[1, 1, 1, 1]},{"id":8,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0]],"name":"Dehaze","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Amount",1],["MinimumTransmission",1],["Strength",1],["HazeColor",3]],"pos":[420, 40],"text":"    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);\n    float dark = min(s.r, min(s.g, s.b));\n    float transmission = max(1.0 - Amount * dark, MinimumTransmission);\n    float3 v = (s.rgb - HazeColor.rgb) / transmission + HazeColor.rgb;\n    return float4(lerp(s.rgb, v, saturate(Strength)), s.a);","value":[0, 0, 0, 0]},{"id":9,"in":[[8, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo 0
//@param Amount float 0.5 -2 2 "Amount"
//@param MinimumTransmission float 0.15 0.01 1 "Minimum Transmission"
//@param Strength float 1 0 1 "Strength"
//@param HazeColor color 1 1 1 1 "Haze Color"
// Imported preset (Effect / Color & Tone): Restores contrast and color density in hazy or washed-out images.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Amount, float MinimumTransmission, float Strength, float3 HazeColor)
{
    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);
    float dark = min(s.r, min(s.g, s.b));
    float transmission = max(1.0 - Amount * dark, MinimumTransmission);
    float3 v = (s.rgb - HazeColor.rgb) / transmission + HazeColor.rgb;
    return float4(lerp(s.rgb, v, saturate(Strength)), s.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Amount, MinimumTransmission, Strength, HazeColor.rgb);
}
