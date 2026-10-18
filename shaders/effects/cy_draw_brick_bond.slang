//@name Brick Bond
//@category Dessin
//@graph {"category":"Dessin","inputs":0,"name":"Brick Bond","nextId":18,"nodes":[{"id":1,"in":[],"op":"uv","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"canvassize","pos":[40, 130],"value":[0, 0, 0, 0]},{"id":3,"in":[],"index":1,"max":128,"min":1,"name":"Columns","op":"param","pos":[40, 220],"text":"Columns","value":[8, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":256,"min":1,"name":"Rows","op":"param","pos":[40, 330],"text":"Rows","value":[16, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":1000,"min":-1000,"name":"Seed","op":"param","pos":[40, 440],"text":"Seed","value":[0, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":0.449999988,"min":0.00100000005,"name":"MortarWidth","op":"param","pos":[40, 550],"text":"Mortar Width","value":[0.0549999997, 0, 0, 0]},{"id":7,"in":[],"index":1,"max":0.200000003,"min":9.99999975e-05,"name":"Softness","op":"param","pos":[40, 660],"text":"Softness","value":[0.00999999978, 0, 0, 0]},{"id":8,"in":[],"index":1,"max":0.5,"min":0.00100000005,"name":"BevelWidth","op":"param","pos":[40, 770],"text":"Bevel Width","value":[0.119999997, 0, 0, 0]},{"id":9,"in":[],"index":1,"name":"BevelDarkening","op":"param","pos":[40, 880],"text":"Bevel Darkening","value":[0.649999976, 0, 0, 0]},{"id":10,"in":[],"index":1,"max":256,"min":1,"name":"GrainScale","op":"param","pos":[40, 990],"text":"Grain Scale","value":[12, 0, 0, 0]},{"id":11,"in":[],"index":1,"name":"GrainAmount","op":"param","pos":[40, 1100],"text":"Grain Amount","value":[0.119999997, 0, 0, 0]},{"id":12,"in":[],"index":1,"name":"Opacity","op":"param","pos":[40, 1210],"text":"Opacity","value":[1, 0, 0, 0]},{"id":13,"in":[],"index":1,"name":"BrickColorA","op":"paramcolor","pos":[40, 1320],"text":"Brick Color A","value":[0.280000001, 0.0549999997, 0.0250000004, 1]},{"id":14,"in":[],"index":1,"name":"BrickColorB","op":"paramcolor","pos":[40, 1430],"text":"Brick Color B","value":[0.550000012, 0.140000001, 0.0549999997, 1]},{"id":15,"in":[],"index":1,"name":"MortarColor","op":"paramcolor","pos":[40, 1540],"text":"Mortar Color","value":[0.280000001, 0.25, 0.200000003, 1]},{"id":16,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0],[11, 0],[12, 0],[13, 0],[14, 0],[15, 0]],"name":"Brick Bond","op":"custom","outs":[["Resultat",4]],"pins":[["UV",2],["CyCanvasSize",4],["Columns",1],["Rows",1],["Seed",1],["MortarWidth",1],["Softness",1],["BevelWidth",1],["BevelDarkening",1],["GrainScale",1],["GrainAmount",1],["Opacity",1],["BrickColorA",3],["BrickColorB",3],["MortarColor",3]],"pos":[420, 40],"text":"    float2 q = UV * float2(max(Columns, 1.0), max(Rows, 1.0));\n    float row = floor(q.y);\n    q.x += fmod(row, 2.0) * .5;\n    float2 id = floor(q);\n    float2 p = frac(q);\n    float h = frac(sin(dot(id + Seed, float2(127.1, 311.7))) * 43758.5453);\n    float2 edge = min(p, 1.0 - p);\n    float mortar = 1.0 - smoothstep(MortarWidth, MortarWidth + Softness, min(edge.x, edge.y));\n    float bevel = saturate(min(edge.x, edge.y) / max(BevelWidth, .0001));\n    float grain = frac(sin(dot(floor(q * GrainScale) + Seed, float2(12.9898, 78.233))) * 43758.5453);\n    float3 brick = lerp(BrickColorA.rgb, BrickColorB.rgb, h) * lerp(1.0 - GrainAmount, 1.0 + GrainAmount, grain) * lerp(BevelDarkening, 1.0, bevel);\n    return float4(lerp(brick, MortarColor.rgb, mortar), Opacity);","value":[0, 0, 0, 0]},{"id":17,"in":[[16, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@generator
//@halo 0
//@param Columns float 8 1 128 "Columns"
//@param Rows float 16 1 256 "Rows"
//@param Seed float 0 -1000 1000 "Seed"
//@param MortarWidth float 0.055 0.001 0.45 "Mortar Width"
//@param Softness float 0.01 0.0001 0.2 "Softness"
//@param BevelWidth float 0.12 0.001 0.5 "Bevel Width"
//@param BevelDarkening float 0.65 0 1 "Bevel Darkening"
//@param GrainScale float 12 1 256 "Grain Scale"
//@param GrainAmount float 0.12 0 1 "Grain Amount"
//@param Opacity float 1 0 1 "Opacity"
//@param BrickColorA color 0.28 0.055 0.025 1 "Brick Color A"
//@param BrickColorB color 0.55 0.14 0.055 1 "Brick Color B"
//@param MortarColor color 0.28 0.25 0.2 1 "Mortar Color"
// Imported preset (Draw / Surface): Seamless running-bond brick generator with mortar, bevel, variation, and surface grain.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Columns, float Rows, float Seed, float MortarWidth, float Softness, float BevelWidth, float BevelDarkening, float GrainScale, float GrainAmount, float Opacity, float3 BrickColorA, float3 BrickColorB, float3 MortarColor)
{
    float2 q = UV * float2(max(Columns, 1.0), max(Rows, 1.0));
    float row = floor(q.y);
    q.x += fmod(row, 2.0) * .5;
    float2 id = floor(q);
    float2 p = frac(q);
    float h = frac(sin(dot(id + Seed, float2(127.1, 311.7))) * 43758.5453);
    float2 edge = min(p, 1.0 - p);
    float mortar = 1.0 - smoothstep(MortarWidth, MortarWidth + Softness, min(edge.x, edge.y));
    float bevel = saturate(min(edge.x, edge.y) / max(BevelWidth, .0001));
    float grain = frac(sin(dot(floor(q * GrainScale) + Seed, float2(12.9898, 78.233))) * 43758.5453);
    float3 brick = lerp(BrickColorA.rgb, BrickColorB.rgb, h) * lerp(1.0 - GrainAmount, 1.0 + GrainAmount, grain) * lerp(BevelDarkening, 1.0, bevel);
    return float4(lerp(brick, MortarColor.rgb, mortar), Opacity);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Columns, Rows, Seed, MortarWidth, Softness, BevelWidth, BevelDarkening, GrainScale, GrainAmount, Opacity, BrickColorA.rgb, BrickColorB.rgb, MortarColor.rgb);
}
