//@name Scratches
//@category Dessin
//@graph {"category":"Dessin","inputs":0,"name":"Scratches","nextId":21,"nodes":[{"id":1,"in":[],"op":"uv","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"canvassize","pos":[40, 130],"value":[0, 0, 0, 0]},{"id":3,"in":[],"index":1,"max":1024,"min":4,"name":"Cells","op":"param","pos":[40, 220],"text":"Cells","value":[72, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":1000,"min":-1000,"name":"Seed","op":"param","pos":[40, 330],"text":"Seed","value":[0, 0, 0, 0]},{"id":5,"in":[],"index":1,"name":"Density","op":"param","pos":[40, 440],"text":"Density","value":[0.140000001, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":6.28299999,"min":-6.28299999,"name":"BaseAngle","op":"param","pos":[40, 550],"text":"Base Angle","value":[0, 0, 0, 0]},{"id":7,"in":[],"index":1,"max":6.28299999,"min":0,"name":"AngleJitter","op":"param","pos":[40, 660],"text":"Angle Jitter","value":[0.349999994, 0, 0, 0]},{"id":8,"in":[],"index":1,"max":0.300000012,"min":0.00100000005,"name":"Width","op":"param","pos":[40, 770],"text":"Width","value":[0.0250000004, 0, 0, 0]},{"id":9,"in":[],"index":1,"max":0.200000003,"min":9.99999975e-05,"name":"Softness","op":"param","pos":[40, 880],"text":"Softness","value":[0.00800000038, 0, 0, 0]},{"id":10,"in":[],"index":1,"max":0.699999988,"min":0.00999999978,"name":"MinLength","op":"param","pos":[40, 990],"text":"Min Length","value":[0.119999997, 0, 0, 0]},{"id":11,"in":[],"index":1,"max":0.800000012,"min":0.00999999978,"name":"MaxLength","op":"param","pos":[40, 1100],"text":"Max Length","value":[0.479999989, 0, 0, 0]},{"id":12,"in":[],"index":1,"name":"Breakup","op":"param","pos":[40, 1210],"text":"Breakup","value":[0.349999994, 0, 0, 0]},{"id":13,"in":[],"index":1,"max":512,"min":1,"name":"BreakupScale","op":"param","pos":[40, 1320],"text":"Breakup Scale","value":[35, 0, 0, 0]},{"id":14,"in":[],"index":1,"name":"BreakupAmount","op":"param","pos":[40, 1430],"text":"Breakup Amount","value":[0.25, 0, 0, 0]},{"id":15,"in":[],"index":1,"name":"ScratchOpacity","op":"param","pos":[40, 1540],"text":"Scratch Opacity","value":[1, 0, 0, 0]},{"id":16,"in":[],"index":1,"name":"BackgroundOpacity","op":"param","pos":[40, 1650],"text":"Background Opacity","value":[0, 0, 0, 0]},{"id":17,"in":[],"index":1,"name":"ScratchColor","op":"paramcolor","pos":[40, 1760],"text":"Scratch Color","value":[1, 1, 1, 1]},{"id":18,"in":[],"index":1,"name":"BackgroundColor","op":"paramcolor","pos":[40, 1870],"text":"Background Color","value":[0, 0, 0, 1]},{"id":19,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0],[11, 0],[12, 0],[13, 0],[14, 0],[15, 0],[16, 0],[17, 0],[18, 0]],"name":"Scratches","op":"custom","outs":[["Resultat",4]],"pins":[["UV",2],["CyCanvasSize",4],["Cells",1],["Seed",1],["Density",1],["BaseAngle",1],["AngleJitter",1],["Width",1],["Softness",1],["MinLength",1],["MaxLength",1],["Breakup",1],["BreakupScale",1],["BreakupAmount",1],["ScratchOpacity",1],["BackgroundOpacity",1],["ScratchColor",3],["BackgroundColor",3]],"pos":[420, 40],"text":"    float2 p = UV * max(Cells, 1.0);\n    float2 id = floor(p);\n    float2 f = frac(p) - .5;\n    float h = frac(sin(dot(id + Seed, float2(127.1, 311.7))) * 43758.5453);\n    float h2 = frac(sin(dot(id + Seed + 9.7, float2(269.5, 183.3))) * 43758.5453);\n    float angle = BaseAngle + (h - .5) * AngleJitter;\n    float c = cos(angle), s = sin(angle);\n    float2 r = float2(c * f.x - s * f.y, s * f.x + c * f.y);\n    float active = step(1.0 - Density, h);\n    float scratch = (1.0 - smoothstep(Width, Width + Softness, abs(r.y))) * step(abs(r.x), lerp(MinLength, MaxLength, h2));\n    float breakup = step(Breakup, frac(sin((r.x + id.x) * BreakupScale + Seed) * 43758.5453));\n    float m = active * scratch * lerp(1.0, breakup, BreakupAmount);\n    return float4(lerp(BackgroundColor.rgb, ScratchColor.rgb, m), lerp(BackgroundOpacity, ScratchOpacity, m));","value":[0, 0, 0, 0]},{"id":20,"in":[[19, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@generator
//@halo 0
//@param Cells float 72 4 1024 "Cells"
//@param Seed float 0 -1000 1000 "Seed"
//@param Density float 0.14 0 1 "Density"
//@param BaseAngle float 0 -6.283 6.283 "Base Angle"
//@param AngleJitter float 0.35 0 6.283 "Angle Jitter"
//@param Width float 0.025 0.001 0.3 "Width"
//@param Softness float 0.008 0.0001 0.2 "Softness"
//@param MinLength float 0.12 0.01 0.7 "Min Length"
//@param MaxLength float 0.48 0.01 0.8 "Max Length"
//@param Breakup float 0.35 0 1 "Breakup"
//@param BreakupScale float 35 1 512 "Breakup Scale"
//@param BreakupAmount float 0.25 0 1 "Breakup Amount"
//@param ScratchOpacity float 1 0 1 "Scratch Opacity"
//@param BackgroundOpacity float 0 0 1 "Background Opacity"
//@param ScratchColor color 1 1 1 1 "Scratch Color"
//@param BackgroundColor color 0 0 0 1 "Background Color"
// Imported preset (Draw / Surface): Seeded multi-scale directional scratch mask with length, width, density, and edge breakup.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Cells, float Seed, float Density, float BaseAngle, float AngleJitter, float Width, float Softness, float MinLength, float MaxLength, float Breakup, float BreakupScale, float BreakupAmount, float ScratchOpacity, float BackgroundOpacity, float3 ScratchColor, float3 BackgroundColor)
{
    float2 p = UV * max(Cells, 1.0);
    float2 id = floor(p);
    float2 f = frac(p) - .5;
    float h = frac(sin(dot(id + Seed, float2(127.1, 311.7))) * 43758.5453);
    float h2 = frac(sin(dot(id + Seed + 9.7, float2(269.5, 183.3))) * 43758.5453);
    float angle = BaseAngle + (h - .5) * AngleJitter;
    float c = cos(angle), s = sin(angle);
    float2 r = float2(c * f.x - s * f.y, s * f.x + c * f.y);
    float active = step(1.0 - Density, h);
    float scratch = (1.0 - smoothstep(Width, Width + Softness, abs(r.y))) * step(abs(r.x), lerp(MinLength, MaxLength, h2));
    float breakup = step(Breakup, frac(sin((r.x + id.x) * BreakupScale + Seed) * 43758.5453));
    float m = active * scratch * lerp(1.0, breakup, BreakupAmount);
    return float4(lerp(BackgroundColor.rgb, ScratchColor.rgb, m), lerp(BackgroundOpacity, ScratchOpacity, m));
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Cells, Seed, Density, BaseAngle, AngleJitter, Width, Softness, MinLength, MaxLength, Breakup, BreakupScale, BreakupAmount, ScratchOpacity, BackgroundOpacity, ScratchColor.rgb, BackgroundColor.rgb);
}
