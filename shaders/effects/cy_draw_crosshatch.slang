//@name Crosshatch
//@category Dessin
//@graph {"category":"Dessin","inputs":0,"name":"Crosshatch","nextId":14,"nodes":[{"id":1,"in":[],"op":"uv","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"canvassize","pos":[40, 130],"value":[0, 0, 0, 0]},{"id":3,"in":[],"index":1,"max":512,"min":2,"name":"Frequency","op":"param","pos":[40, 220],"text":"Frequency","value":[28, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":6.28299999,"min":-6.28299999,"name":"Angle","op":"param","pos":[40, 330],"text":"Angle","value":[0.449999988, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":0.49000001,"min":0.00100000005,"name":"Width","op":"param","pos":[40, 440],"text":"Width","value":[0.0799999982, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":0.25,"min":9.99999975e-05,"name":"Softness","op":"param","pos":[40, 550],"text":"Softness","value":[0.0250000004, 0, 0, 0]},{"id":7,"in":[],"index":1,"name":"Tone","op":"param","pos":[40, 660],"text":"Tone","value":[0.720000029, 0, 0, 0]},{"id":8,"in":[],"index":1,"name":"InkOpacity","op":"param","pos":[40, 770],"text":"Ink Opacity","value":[1, 0, 0, 0]},{"id":9,"in":[],"index":1,"name":"PaperOpacity","op":"param","pos":[40, 880],"text":"Paper Opacity","value":[1, 0, 0, 0]},{"id":10,"in":[],"index":1,"name":"InkColor","op":"paramcolor","pos":[40, 990],"text":"Ink Color","value":[0.0149999997, 0.0120000001, 0.00999999978, 1]},{"id":11,"in":[],"index":1,"name":"PaperColor","op":"paramcolor","pos":[40, 1100],"text":"Paper Color","value":[0.75, 0.680000007, 0.550000012, 1]},{"id":12,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0],[11, 0]],"name":"Crosshatch","op":"custom","outs":[["Resultat",4]],"pins":[["UV",2],["CyCanvasSize",4],["Frequency",1],["Angle",1],["Width",1],["Softness",1],["Tone",1],["InkOpacity",1],["PaperOpacity",1],["InkColor",3],["PaperColor",3]],"pos":[420, 40],"text":"    float2 p = UV * max(Frequency, 1.0);\n    float c = cos(Angle), s = sin(Angle);\n    float2 r = float2(c * p.x - s * p.y, s * p.x + c * p.y);\n    float l0 = 1.0 - smoothstep(Width, Width + Softness, abs(frac(r.x) - 0.5));\n    float l1 = 1.0 - smoothstep(Width, Width + Softness, abs(frac(r.y) - 0.5));\n    float l2 = 1.0 - smoothstep(Width, Width + Softness, abs(frac((r.x + r.y) * 0.707) - 0.5));\n    float l3 = 1.0 - smoothstep(Width, Width + Softness, abs(frac((r.x - r.y) * 0.707) - 0.5));\n    float tone = saturate(Tone);\n    float ink = max(l0 * step(0.2, tone), max(l1 * step(0.45, tone), max(l2 * step(0.7, tone), l3 * step(0.9, tone))));\n    return float4(lerp(PaperColor.rgb, InkColor.rgb, ink), lerp(PaperOpacity, InkOpacity, ink));","value":[0, 0, 0, 0]},{"id":13,"in":[[12, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@generator
//@halo 0
//@param Frequency float 28 2 512 "Frequency"
//@param Angle float 0.45 -6.283 6.283 "Angle"
//@param Width float 0.08 0.001 0.49 "Width"
//@param Softness float 0.025 0.0001 0.25 "Softness"
//@param Tone float 0.72 0 1 "Tone"
//@param InkOpacity float 1 0 1 "Ink Opacity"
//@param PaperOpacity float 1 0 1 "Paper Opacity"
//@param InkColor color 0.015 0.012 0.01 1 "Ink Color"
//@param PaperColor color 0.75 0.68 0.55 1 "Paper Color"
// Imported preset (Draw / Pattern): Layered crosshatch generator with tone-controlled stroke density and paper color.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Frequency, float Angle, float Width, float Softness, float Tone, float InkOpacity, float PaperOpacity, float3 InkColor, float3 PaperColor)
{
    float2 p = UV * max(Frequency, 1.0);
    float c = cos(Angle), s = sin(Angle);
    float2 r = float2(c * p.x - s * p.y, s * p.x + c * p.y);
    float l0 = 1.0 - smoothstep(Width, Width + Softness, abs(frac(r.x) - 0.5));
    float l1 = 1.0 - smoothstep(Width, Width + Softness, abs(frac(r.y) - 0.5));
    float l2 = 1.0 - smoothstep(Width, Width + Softness, abs(frac((r.x + r.y) * 0.707) - 0.5));
    float l3 = 1.0 - smoothstep(Width, Width + Softness, abs(frac((r.x - r.y) * 0.707) - 0.5));
    float tone = saturate(Tone);
    float ink = max(l0 * step(0.2, tone), max(l1 * step(0.45, tone), max(l2 * step(0.7, tone), l3 * step(0.9, tone))));
    return float4(lerp(PaperColor.rgb, InkColor.rgb, ink), lerp(PaperOpacity, InkOpacity, ink));
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Frequency, Angle, Width, Softness, Tone, InkOpacity, PaperOpacity, InkColor.rgb, PaperColor.rgb);
}
