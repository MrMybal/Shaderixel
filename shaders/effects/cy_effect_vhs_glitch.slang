//@name VHS Glitch
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"VHS Glitch","nextId":16,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":4096,"min":8,"name":"LineDensity","op":"param","pos":[40, 330],"text":"Line Density","value":[240, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":100,"min":-100,"name":"Phase","op":"param","pos":[40, 440],"text":"Phase","value":[0, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":0.999000013,"min":0,"name":"GlitchThreshold","op":"param","pos":[40, 550],"text":"Glitch Threshold","value":[0.870000005, 0, 0, 0]},{"id":7,"in":[],"index":1,"max":16,"min":0.100000001,"name":"GlitchSharpness","op":"param","pos":[40, 660],"text":"Glitch Sharpness","value":[2, 0, 0, 0]},{"id":8,"in":[],"index":1,"max":0.5,"min":0,"name":"HorizontalJitter","op":"param","pos":[40, 770],"text":"Horizontal Jitter","value":[0.0799999982, 0, 0, 0]},{"id":9,"in":[],"index":1,"max":2048,"min":0,"name":"TrackingFrequency","op":"param","pos":[40, 880],"text":"Tracking Frequency","value":[85, 0, 0, 0]},{"id":10,"in":[],"index":1,"max":0.100000001,"min":0,"name":"TrackingAmount","op":"param","pos":[40, 990],"text":"Tracking Amount","value":[0.00300000003, 0, 0, 0]},{"id":11,"in":[],"index":1,"max":64,"min":0,"name":"ColorDelay","op":"param","pos":[40, 1100],"text":"Color Delay","value":[3, 0, 0, 0]},{"id":12,"in":[],"index":1,"name":"Scanlines","op":"param","pos":[40, 1210],"text":"Scanlines","value":[0.25, 0, 0, 0]},{"id":13,"in":[],"index":1,"name":"Dropout","op":"param","pos":[40, 1320],"text":"Dropout","value":[0.349999994, 0, 0, 0]},{"id":14,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0],[11, 0],[12, 0],[13, 0]],"name":"VHS Glitch","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["LineDensity",1],["Phase",1],["GlitchThreshold",1],["GlitchSharpness",1],["HorizontalJitter",1],["TrackingFrequency",1],["TrackingAmount",1],["ColorDelay",1],["Scanlines",1],["Dropout",1]],"pos":[420, 40],"text":"    float scanLine = floor(UV.y * max(LineDensity, 1.0));\n    float n = frac(sin(scanLine * 12.9898 + Phase * 7.13) * 43758.5453);\n    float burst = pow(saturate((n - GlitchThreshold) / max(1.0 - GlitchThreshold, .0001)), GlitchSharpness);\n    float jitter = (n - .5) * HorizontalJitter + sin(UV.y * TrackingFrequency + Phase) * TrackingAmount;\n    float2 uv = UV + float2(jitter * burst, 0);\n    float delay = ColorDelay / max(CyCanvasSize.x, 1.0);\n    float r = Texture2DSample(CyInput, CyInputSampler, uv + float2(delay, 0)).r;\n    float g = Texture2DSample(CyInput, CyInputSampler, uv).g;\n    float b = Texture2DSample(CyInput, CyInputSampler, uv - float2(delay, 0)).b;\n    float scan = lerp(1.0, 0.75 + 0.25 * sin(UV.y * CyCanvasSize.y * 3.14159), Scanlines);\n    float dropout = 1.0 - burst * Dropout;\n    float4 src = Texture2DSample(CyInput, CyInputSampler, uv);\n    return float4(float3(r, g, b) * scan * dropout, src.a);","value":[0, 0, 0, 0]},{"id":15,"in":[[14, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo full
//@param LineDensity float 240 8 4096 "Line Density"
//@param Phase float 0 -100 100 "Phase"
//@param GlitchThreshold float 0.87 0 0.999 "Glitch Threshold"
//@param GlitchSharpness float 2 0.1 16 "Glitch Sharpness"
//@param HorizontalJitter float 0.08 0 0.5 "Horizontal Jitter"
//@param TrackingFrequency float 85 0 2048 "Tracking Frequency"
//@param TrackingAmount float 0.003 0 0.1 "Tracking Amount"
//@param ColorDelay float 3 0 64 "Color Delay"
//@param Scanlines float 0.25 0 1 "Scanlines"
//@param Dropout float 0.35 0 1 "Dropout"
// Imported preset (Effect / Stylize): Analog VHS displacement with tracking noise, color delay, dropout, and scanline controls.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float LineDensity, float Phase, float GlitchThreshold, float GlitchSharpness, float HorizontalJitter, float TrackingFrequency, float TrackingAmount, float ColorDelay, float Scanlines, float Dropout)
{
    float scanLine = floor(UV.y * max(LineDensity, 1.0));
    float n = frac(sin(scanLine * 12.9898 + Phase * 7.13) * 43758.5453);
    float burst = pow(saturate((n - GlitchThreshold) / max(1.0 - GlitchThreshold, .0001)), GlitchSharpness);
    float jitter = (n - .5) * HorizontalJitter + sin(UV.y * TrackingFrequency + Phase) * TrackingAmount;
    float2 uv = UV + float2(jitter * burst, 0);
    float delay = ColorDelay / max(CyCanvasSize.x, 1.0);
    float r = Texture2DSample(CyInput, CyInputSampler, uv + float2(delay, 0)).r;
    float g = Texture2DSample(CyInput, CyInputSampler, uv).g;
    float b = Texture2DSample(CyInput, CyInputSampler, uv - float2(delay, 0)).b;
    float scan = lerp(1.0, 0.75 + 0.25 * sin(UV.y * CyCanvasSize.y * 3.14159), Scanlines);
    float dropout = 1.0 - burst * Dropout;
    float4 src = Texture2DSample(CyInput, CyInputSampler, uv);
    return float4(float3(r, g, b) * scan * dropout, src.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), LineDensity, Phase, GlitchThreshold, GlitchSharpness, HorizontalJitter, TrackingFrequency, TrackingAmount, ColorDelay, Scanlines, Dropout);
}
