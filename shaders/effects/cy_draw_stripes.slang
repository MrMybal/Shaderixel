//@name Stripes
//@category Dessin
//@graph {"category":"Dessin","inputs":0,"name":"Stripes","nextId":14,"nodes":[{"id":1,"in":[],"op":"uv","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"canvassize","pos":[40, 130],"value":[0, 0, 0, 0]},{"id":3,"in":[],"index":1,"max":512,"min":1,"name":"Frequency","op":"param","pos":[40, 220],"text":"Frequency","value":[16, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":6.28299999,"min":-6.28299999,"name":"Angle","op":"param","pos":[40, 330],"text":"Angle","value":[0, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":10,"min":-10,"name":"Phase","op":"param","pos":[40, 440],"text":"Phase","value":[0, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":0.99000001,"min":0.00999999978,"name":"Duty","op":"param","pos":[40, 550],"text":"Duty","value":[0.5, 0, 0, 0]},{"id":7,"in":[],"index":1,"max":0.25,"min":9.99999975e-05,"name":"Softness","op":"param","pos":[40, 660],"text":"Softness","value":[0.00999999978, 0, 0, 0]},{"id":8,"in":[],"index":1,"name":"OpacityA","op":"param","pos":[40, 770],"text":"Opacity A","value":[1, 0, 0, 0]},{"id":9,"in":[],"index":1,"name":"OpacityB","op":"param","pos":[40, 880],"text":"Opacity B","value":[1, 0, 0, 0]},{"id":10,"in":[],"index":1,"name":"ColorA","op":"paramcolor","pos":[40, 990],"text":"Color A","value":[1, 1, 1, 1]},{"id":11,"in":[],"index":1,"name":"ColorB","op":"paramcolor","pos":[40, 1100],"text":"Color B","value":[0, 0, 0, 1]},{"id":12,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0],[11, 0]],"name":"Stripes","op":"custom","outs":[["Resultat",4]],"pins":[["UV",2],["CyCanvasSize",4],["Frequency",1],["Angle",1],["Phase",1],["Duty",1],["Softness",1],["OpacityA",1],["OpacityB",1],["ColorA",3],["ColorB",3]],"pos":[420, 40],"text":"    float2 p = UV - 0.5;\n    float c = cos(Angle);\n    float s = sin(Angle);\n    float u = dot(p, float2(c, s)) * Frequency + Phase;\n    float v = frac(u);\n    float edge = max(Softness, 0.0001);\n    float m = 1.0 - smoothstep(Duty - edge, Duty + edge, v);\n    return float4(lerp(ColorB.rgb, ColorA.rgb, m), lerp(OpacityB, OpacityA, m));","value":[0, 0, 0, 0]},{"id":13,"in":[[12, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@generator
//@halo 0
//@param Frequency float 16 1 512 "Frequency"
//@param Angle float 0 -6.283 6.283 "Angle"
//@param Phase float 0 -10 10 "Phase"
//@param Duty float 0.5 0.01 0.99 "Duty"
//@param Softness float 0.01 0.0001 0.25 "Softness"
//@param OpacityA float 1 0 1 "Opacity A"
//@param OpacityB float 1 0 1 "Opacity B"
//@param ColorA color 1 1 1 1 "Color A"
//@param ColorB color 0 0 0 1 "Color B"
// Imported preset (Draw / Shape & Gradient): Rotatable antialiased stripe generator.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Frequency, float Angle, float Phase, float Duty, float Softness, float OpacityA, float OpacityB, float3 ColorA, float3 ColorB)
{
    float2 p = UV - 0.5;
    float c = cos(Angle);
    float s = sin(Angle);
    float u = dot(p, float2(c, s)) * Frequency + Phase;
    float v = frac(u);
    float edge = max(Softness, 0.0001);
    float m = 1.0 - smoothstep(Duty - edge, Duty + edge, v);
    return float4(lerp(ColorB.rgb, ColorA.rgb, m), lerp(OpacityB, OpacityA, m));
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Frequency, Angle, Phase, Duty, Softness, OpacityA, OpacityB, ColorA.rgb, ColorB.rgb);
}
