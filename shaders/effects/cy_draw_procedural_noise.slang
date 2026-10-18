//@name Procedural Noise
//@category Dessin
//@graph {"category":"Dessin","inputs":0,"name":"Procedural Noise","nextId":12,"nodes":[{"id":1,"in":[],"op":"uv","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"canvassize","pos":[40, 130],"value":[0, 0, 0, 0]},{"id":3,"in":[],"index":1,"max":1024,"min":1,"name":"Scale","op":"param","pos":[40, 220],"text":"Scale","value":[16, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":1000,"min":-1000,"name":"Seed","op":"param","pos":[40, 330],"text":"Seed","value":[0, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":8,"min":0,"name":"Contrast","op":"param","pos":[40, 440],"text":"Contrast","value":[1, 0, 0, 0]},{"id":6,"in":[],"index":1,"name":"OpacityA","op":"param","pos":[40, 550],"text":"Opacity A","value":[1, 0, 0, 0]},{"id":7,"in":[],"index":1,"name":"OpacityB","op":"param","pos":[40, 660],"text":"Opacity B","value":[1, 0, 0, 0]},{"id":8,"in":[],"index":1,"name":"ColorA","op":"paramcolor","pos":[40, 770],"text":"Color A","value":[0, 0, 0, 1]},{"id":9,"in":[],"index":1,"name":"ColorB","op":"paramcolor","pos":[40, 880],"text":"Color B","value":[1, 1, 1, 1]},{"id":10,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0]],"name":"Procedural Noise","op":"custom","outs":[["Resultat",4]],"pins":[["UV",2],["CyCanvasSize",4],["Scale",1],["Seed",1],["Contrast",1],["OpacityA",1],["OpacityB",1],["ColorA",3],["ColorB",3]],"pos":[420, 40],"text":"    float2 p = UV * max(Scale, 1.0) + Seed;\n    float2 i = floor(p);\n    float2 f = frac(p);\n    f = f * f * (3.0 - 2.0 * f);\n    float h00 = frac(sin(dot(i, float2(127.1, 311.7))) * 43758.5453);\n    float h10 = frac(sin(dot(i + float2(1, 0), float2(127.1, 311.7))) * 43758.5453);\n    float h01 = frac(sin(dot(i + float2(0, 1), float2(127.1, 311.7))) * 43758.5453);\n    float h11 = frac(sin(dot(i + 1.0, float2(127.1, 311.7))) * 43758.5453);\n    float n = lerp(lerp(h00, h10, f.x), lerp(h01, h11, f.x), f.y);\n    n = saturate((n - 0.5) * Contrast + 0.5);\n    return float4(lerp(ColorA, ColorB, n), lerp(OpacityA, OpacityB, n));","value":[0, 0, 0, 0]},{"id":11,"in":[[10, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@generator
//@halo 0
//@param Scale float 16 1 1024 "Scale"
//@param Seed float 0 -1000 1000 "Seed"
//@param Contrast float 1 0 8 "Contrast"
//@param OpacityA float 1 0 1 "Opacity A"
//@param OpacityB float 1 0 1 "Opacity B"
//@param ColorA color 0 0 0 1 "Color A"
//@param ColorB color 1 1 1 1 "Color B"
// Imported preset (Draw / Noise): Fast deterministic value noise for masks, grain, and procedural sources.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Scale, float Seed, float Contrast, float OpacityA, float OpacityB, float3 ColorA, float3 ColorB)
{
    float2 p = UV * max(Scale, 1.0) + Seed;
    float2 i = floor(p);
    float2 f = frac(p);
    f = f * f * (3.0 - 2.0 * f);
    float h00 = frac(sin(dot(i, float2(127.1, 311.7))) * 43758.5453);
    float h10 = frac(sin(dot(i + float2(1, 0), float2(127.1, 311.7))) * 43758.5453);
    float h01 = frac(sin(dot(i + float2(0, 1), float2(127.1, 311.7))) * 43758.5453);
    float h11 = frac(sin(dot(i + 1.0, float2(127.1, 311.7))) * 43758.5453);
    float n = lerp(lerp(h00, h10, f.x), lerp(h01, h11, f.x), f.y);
    n = saturate((n - 0.5) * Contrast + 0.5);
    return float4(lerp(ColorA, ColorB, n), lerp(OpacityA, OpacityB, n));
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Scale, Seed, Contrast, OpacityA, OpacityB, ColorA.rgb, ColorB.rgb);
}
