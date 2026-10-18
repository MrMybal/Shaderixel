//@name Aurora Bands
//@category Dessin
//@graph {"category":"Dessin","inputs":0,"name":"Aurora Bands","nextId":18,"nodes":[{"id":1,"in":[],"op":"uv","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"canvassize","pos":[40, 130],"value":[0, 0, 0, 0]},{"id":3,"in":[],"index":1,"max":64,"min":0.100000001,"name":"Frequency","op":"param","pos":[40, 220],"text":"Frequency","value":[8, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":20,"min":-20,"name":"Phase","op":"param","pos":[40, 330],"text":"Phase","value":[0, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":0.5,"min":0,"name":"Amplitude","op":"param","pos":[40, 440],"text":"Amplitude","value":[0.0799999982, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":0.5,"min":0.00100000005,"name":"Width","op":"param","pos":[40, 550],"text":"Width","value":[0.0799999982, 0, 0, 0]},{"id":7,"in":[],"index":1,"max":8,"min":0.0500000007,"name":"Sharpness","op":"param","pos":[40, 660],"text":"Sharpness","value":[1.39999998, 0, 0, 0]},{"id":8,"in":[],"index":1,"max":256,"min":1,"name":"Striation","op":"param","pos":[40, 770],"text":"Striation","value":[18, 0, 0, 0]},{"id":9,"in":[],"index":1,"name":"StriationAmount","op":"param","pos":[40, 880],"text":"Striation Amount","value":[0.550000012, 0, 0, 0]},{"id":10,"in":[],"index":1,"name":"Horizon","op":"param","pos":[40, 990],"text":"Horizon","value":[0.519999981, 0, 0, 0]},{"id":11,"in":[],"index":1,"max":2,"min":0.00100000005,"name":"HorizonFalloff","op":"param","pos":[40, 1100],"text":"Horizon Falloff","value":[0.649999976, 0, 0, 0]},{"id":12,"in":[],"index":1,"max":64,"min":0,"name":"Intensity","op":"param","pos":[40, 1210],"text":"Intensity","value":[3, 0, 0, 0]},{"id":13,"in":[],"index":1,"name":"Opacity","op":"param","pos":[40, 1320],"text":"Opacity","value":[1, 0, 0, 0]},{"id":14,"in":[],"index":1,"name":"ColorA","op":"paramcolor","pos":[40, 1430],"text":"Color A","value":[0.0500000007, 2, 0.449999988, 1]},{"id":15,"in":[],"index":1,"name":"ColorB","op":"paramcolor","pos":[40, 1540],"text":"Color B","value":[0.400000006, 0.0799999982, 2.5, 1]},{"id":16,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0],[11, 0],[12, 0],[13, 0],[14, 0],[15, 0]],"name":"Aurora Bands","op":"custom","outs":[["Resultat",4]],"pins":[["UV",2],["CyCanvasSize",4],["Frequency",1],["Phase",1],["Amplitude",1],["Width",1],["Sharpness",1],["Striation",1],["StriationAmount",1],["Horizon",1],["HorizonFalloff",1],["Intensity",1],["Opacity",1],["ColorA",3],["ColorB",3]],"pos":[420, 40],"text":"    float2 p = UV;\n    float wave = 0.5 + Amplitude * (sin(p.x * Frequency + Phase) + 0.45 * sin(p.x * Frequency * 2.17 - Phase * 1.3));\n    float d = abs(p.y - wave);\n    float ribbon = exp(-d / max(Width, 0.0001));\n    float striation = 0.55 + 0.45 * sin((p.x * Striation + p.y * 7.0 + Phase) * 6.283);\n    float horizon = pow(saturate(1.0 - abs(p.y - Horizon) / max(HorizonFalloff, 0.0001)), 2.0);\n    float v = pow(saturate(ribbon * lerp(1.0, striation, StriationAmount) * horizon), max(Sharpness, 0.05));\n    float3 rgb = lerp(ColorA.rgb, ColorB.rgb, saturate(p.x + v * 0.25)) * v * Intensity;\n    return float4(rgb, saturate(v * Opacity));","value":[0, 0, 0, 0]},{"id":17,"in":[[16, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@generator
//@halo 0
//@param Frequency float 8 0.1 64 "Frequency"
//@param Phase float 0 -20 20 "Phase"
//@param Amplitude float 0.08 0 0.5 "Amplitude"
//@param Width float 0.08 0.001 0.5 "Width"
//@param Sharpness float 1.4 0.05 8 "Sharpness"
//@param Striation float 18 1 256 "Striation"
//@param StriationAmount float 0.55 0 1 "Striation Amount"
//@param Horizon float 0.52 0 1 "Horizon"
//@param HorizonFalloff float 0.65 0.001 2 "Horizon Falloff"
//@param Intensity float 3 0 64 "Intensity"
//@param Opacity float 1 0 1 "Opacity"
//@param ColorA color 0.05 2 0.45 1 "Color A"
//@param ColorB color 0.4 0.08 2.5 1 "Color B"
// Imported preset (Draw / Light & Space): Layered HDR aurora ribbons with turbulence, glow, and horizon falloff.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Frequency, float Phase, float Amplitude, float Width, float Sharpness, float Striation, float StriationAmount, float Horizon, float HorizonFalloff, float Intensity, float Opacity, float3 ColorA, float3 ColorB)
{
    float2 p = UV;
    float wave = 0.5 + Amplitude * (sin(p.x * Frequency + Phase) + 0.45 * sin(p.x * Frequency * 2.17 - Phase * 1.3));
    float d = abs(p.y - wave);
    float ribbon = exp(-d / max(Width, 0.0001));
    float striation = 0.55 + 0.45 * sin((p.x * Striation + p.y * 7.0 + Phase) * 6.283);
    float horizon = pow(saturate(1.0 - abs(p.y - Horizon) / max(HorizonFalloff, 0.0001)), 2.0);
    float v = pow(saturate(ribbon * lerp(1.0, striation, StriationAmount) * horizon), max(Sharpness, 0.05));
    float3 rgb = lerp(ColorA.rgb, ColorB.rgb, saturate(p.x + v * 0.25)) * v * Intensity;
    return float4(rgb, saturate(v * Opacity));
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Frequency, Phase, Amplitude, Width, Sharpness, Striation, StriationAmount, Horizon, HorizonFalloff, Intensity, Opacity, ColorA.rgb, ColorB.rgb);
}
