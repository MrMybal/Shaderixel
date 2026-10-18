//@name Mosaic Glass
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Mosaic Glass","nextId":15,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":512,"min":2,"name":"Cells","op":"param","pos":[40, 330],"text":"Cells","value":[24, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":1000,"min":-1000,"name":"Seed","op":"param","pos":[40, 440],"text":"Seed","value":[0, 0, 0, 0]},{"id":6,"in":[],"index":1,"name":"CellJitter","op":"param","pos":[40, 550],"text":"Cell Jitter","value":[0.550000012, 0, 0, 0]},{"id":7,"in":[],"index":1,"max":0.449999988,"min":0.00100000005,"name":"GroutWidth","op":"param","pos":[40, 660],"text":"Grout Width","value":[0.0599999987, 0, 0, 0]},{"id":8,"in":[],"index":1,"max":0.200000003,"min":9.99999975e-05,"name":"Softness","op":"param","pos":[40, 770],"text":"Softness","value":[0.0149999997, 0, 0, 0]},{"id":9,"in":[],"index":1,"max":8,"min":0.0500000007,"name":"BevelPower","op":"param","pos":[40, 880],"text":"Bevel Power","value":[0.699999988, 0, 0, 0]},{"id":10,"in":[],"index":1,"max":2,"min":0,"name":"BevelAmount","op":"param","pos":[40, 990],"text":"Bevel Amount","value":[0.25, 0, 0, 0]},{"id":11,"in":[],"index":1,"name":"GroutOpacity","op":"param","pos":[40, 1100],"text":"Grout Opacity","value":[1, 0, 0, 0]},{"id":12,"in":[],"index":1,"name":"GroutColor","op":"paramcolor","pos":[40, 1210],"text":"Grout Color","value":[0.0149999997, 0.0149999997, 0.0179999992, 1]},{"id":13,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0],[11, 0],[12, 0]],"name":"Mosaic Glass","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Cells",1],["Seed",1],["CellJitter",1],["GroutWidth",1],["Softness",1],["BevelPower",1],["BevelAmount",1],["GroutOpacity",1],["GroutColor",3]],"pos":[420, 40],"text":"    float2 q = UV * max(Cells, 1.0);\n    float2 id = floor(q);\n    float2 f = frac(q) - .5;\n    float h = frac(sin(dot(id + Seed, float2(127.1, 311.7))) * 43758.5453);\n    float2 jitter = float2(h - .5, frac(h * 19.17) - .5) * CellJitter;\n    float2 sampleUV = (id + .5 + jitter) / max(Cells, 1.0);\n    float4 s = Texture2DSample(CyInput, CyInputSampler, sampleUV);\n    float edge = max(abs(f.x), abs(f.y));\n    float grout = smoothstep(.5 - GroutWidth, .5 - GroutWidth + Softness, edge);\n    float bevel = pow(saturate(1.0 - edge * 2.0), BevelPower);\n    float3 glass = s.rgb * lerp(1.0, bevel, BevelAmount);\n    return float4(lerp(glass, GroutColor.rgb, grout), lerp(s.a, GroutOpacity, grout));","value":[0, 0, 0, 0]},{"id":14,"in":[[13, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo full
//@param Cells float 24 2 512 "Cells"
//@param Seed float 0 -1000 1000 "Seed"
//@param CellJitter float 0.55 0 1 "Cell Jitter"
//@param GroutWidth float 0.06 0.001 0.45 "Grout Width"
//@param Softness float 0.015 0.0001 0.2 "Softness"
//@param BevelPower float 0.7 0.05 8 "Bevel Power"
//@param BevelAmount float 0.25 0 2 "Bevel Amount"
//@param GroutOpacity float 1 0 1 "Grout Opacity"
//@param GroutColor color 0.015 0.015 0.018 1 "Grout Color"
// Imported preset (Effect / Stylize): Cellular stained-glass mosaic with seeded sampling, bevel shading, and grout color.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Cells, float Seed, float CellJitter, float GroutWidth, float Softness, float BevelPower, float BevelAmount, float GroutOpacity, float3 GroutColor)
{
    float2 q = UV * max(Cells, 1.0);
    float2 id = floor(q);
    float2 f = frac(q) - .5;
    float h = frac(sin(dot(id + Seed, float2(127.1, 311.7))) * 43758.5453);
    float2 jitter = float2(h - .5, frac(h * 19.17) - .5) * CellJitter;
    float2 sampleUV = (id + .5 + jitter) / max(Cells, 1.0);
    float4 s = Texture2DSample(CyInput, CyInputSampler, sampleUV);
    float edge = max(abs(f.x), abs(f.y));
    float grout = smoothstep(.5 - GroutWidth, .5 - GroutWidth + Softness, edge);
    float bevel = pow(saturate(1.0 - edge * 2.0), BevelPower);
    float3 glass = s.rgb * lerp(1.0, bevel, BevelAmount);
    return float4(lerp(glass, GroutColor.rgb, grout), lerp(s.a, GroutOpacity, grout));
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Cells, Seed, CellJitter, GroutWidth, Softness, BevelPower, BevelAmount, GroutOpacity, GroutColor.rgb);
}
