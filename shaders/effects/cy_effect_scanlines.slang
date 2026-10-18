//@name Scanlines
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Scanlines","nextId":11,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":8192,"min":1,"name":"Frequency","op":"param","pos":[40, 330],"text":"Frequency","value":[540, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":10,"min":-10,"name":"Phase","op":"param","pos":[40, 440],"text":"Phase","value":[0, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":32,"min":0.100000001,"name":"Sharpness","op":"param","pos":[40, 550],"text":"Sharpness","value":[3, 0, 0, 0]},{"id":7,"in":[],"index":1,"name":"Intensity","op":"param","pos":[40, 660],"text":"Intensity","value":[0.25, 0, 0, 0]},{"id":8,"in":[],"index":1,"name":"LineColor","op":"paramcolor","pos":[40, 770],"text":"Line Color","value":[0, 0, 0, 1]},{"id":9,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0]],"name":"Scanlines","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Frequency",1],["Phase",1],["Sharpness",1],["Intensity",1],["LineColor",3]],"pos":[420, 40],"text":"    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);\n    float scanline = 0.5 + 0.5 * cos((UV.y * Frequency + Phase) * 6.2831853);\n    scanline = pow(scanline, max(Sharpness, 0.1));\n    float3 v = s.rgb * lerp(1.0, LineColor.rgb, scanline * Intensity);\n    return float4(v, s.a);","value":[0, 0, 0, 0]},{"id":10,"in":[[9, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo 0
//@param Frequency float 540 1 8192 "Frequency"
//@param Phase float 0 -10 10 "Phase"
//@param Sharpness float 3 0.1 32 "Sharpness"
//@param Intensity float 0.25 0 1 "Intensity"
//@param LineColor color 0 0 0 1 "Line Color"
// Imported preset (Effect / Stylize): CRT-style scanlines with phase and tint.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Frequency, float Phase, float Sharpness, float Intensity, float3 LineColor)
{
    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);
    float scanline = 0.5 + 0.5 * cos((UV.y * Frequency + Phase) * 6.2831853);
    scanline = pow(scanline, max(Sharpness, 0.1));
    float3 v = s.rgb * lerp(1.0, LineColor.rgb, scanline * Intensity);
    return float4(v, s.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Frequency, Phase, Sharpness, Intensity, LineColor.rgb);
}
