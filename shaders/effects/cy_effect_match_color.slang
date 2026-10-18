//@name Match Color
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Match Color","nextId":11,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"name":"LuminanceStrength","op":"param","pos":[40, 330],"text":"Luminance Strength","value":[1, 0, 0, 0]},{"id":5,"in":[],"index":1,"name":"ColorStrength","op":"param","pos":[40, 440],"text":"Color Strength","value":[1, 0, 0, 0]},{"id":6,"in":[],"index":1,"name":"Strength","op":"param","pos":[40, 550],"text":"Strength","value":[1, 0, 0, 0]},{"id":7,"in":[],"index":1,"name":"SourceAverage","op":"paramcolor","pos":[40, 660],"text":"Source Average","value":[0.5, 0.5, 0.5, 1]},{"id":8,"in":[],"index":1,"name":"TargetAverage","op":"paramcolor","pos":[40, 770],"text":"Target Average","value":[0.5, 0.5, 0.5, 1]},{"id":9,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0]],"name":"Match Color","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["LuminanceStrength",1],["ColorStrength",1],["Strength",1],["SourceAverage",3],["TargetAverage",3]],"pos":[420, 40],"text":"    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);\n    float sl = max(dot(SourceAverage.rgb, float3(.2126, .7152, .0722)), 0.0001);\n    float tl = dot(TargetAverage.rgb, float3(.2126, .7152, .0722));\n    float3 lum = s.rgb * (tl / sl);\n    float3 chroma = s.rgb * TargetAverage.rgb / max(SourceAverage.rgb, 0.0001);\n    float3 v = lerp(s.rgb, lum, saturate(LuminanceStrength));\n    v = lerp(v, chroma, saturate(ColorStrength));\n    return float4(lerp(s.rgb, v, saturate(Strength)), s.a);","value":[0, 0, 0, 0]},{"id":10,"in":[[9, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo 0
//@param LuminanceStrength float 1 0 1 "Luminance Strength"
//@param ColorStrength float 1 0 1 "Color Strength"
//@param Strength float 1 0 1 "Strength"
//@param SourceAverage color 0.5 0.5 0.5 1 "Source Average"
//@param TargetAverage color 0.5 0.5 0.5 1 "Target Average"
// Imported preset (Effect / Color & Tone): Matches the image toward a target average color with luminance and color strengths.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float LuminanceStrength, float ColorStrength, float Strength, float3 SourceAverage, float3 TargetAverage)
{
    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);
    float sl = max(dot(SourceAverage.rgb, float3(.2126, .7152, .0722)), 0.0001);
    float tl = dot(TargetAverage.rgb, float3(.2126, .7152, .0722));
    float3 lum = s.rgb * (tl / sl);
    float3 chroma = s.rgb * TargetAverage.rgb / max(SourceAverage.rgb, 0.0001);
    float3 v = lerp(s.rgb, lum, saturate(LuminanceStrength));
    v = lerp(v, chroma, saturate(ColorStrength));
    return float4(lerp(s.rgb, v, saturate(Strength)), s.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), LuminanceStrength, ColorStrength, Strength, SourceAverage.rgb, TargetAverage.rgb);
}
