//@name Hex Grid
//@category Dessin
//@graph {"category":"Dessin","inputs":0,"name":"Hex Grid","nextId":14,"nodes":[{"id":1,"in":[],"op":"uv","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"canvassize","pos":[40, 130],"value":[0, 0, 0, 0]},{"id":3,"in":[],"index":1,"max":256,"min":1,"name":"Scale","op":"param","pos":[40, 220],"text":"Scale","value":[12, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":0.25,"min":0.00100000005,"name":"LineWidth","op":"param","pos":[40, 330],"text":"Line Width","value":[0.0250000004, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":0.100000001,"min":9.99999975e-05,"name":"Softness","op":"param","pos":[40, 440],"text":"Softness","value":[0.00800000038, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":0.300000012,"min":0.00100000005,"name":"NodeSize","op":"param","pos":[40, 550],"text":"Node Size","value":[0.0549999997, 0, 0, 0]},{"id":7,"in":[],"index":1,"name":"NodeIntensity","op":"param","pos":[40, 660],"text":"Node Intensity","value":[0.5, 0, 0, 0]},{"id":8,"in":[],"index":1,"name":"LineOpacity","op":"param","pos":[40, 770],"text":"Line Opacity","value":[1, 0, 0, 0]},{"id":9,"in":[],"index":1,"name":"CellOpacity","op":"param","pos":[40, 880],"text":"Cell Opacity","value":[1, 0, 0, 0]},{"id":10,"in":[],"index":1,"name":"LineColor","op":"paramcolor","pos":[40, 990],"text":"Line Color","value":[0.0500000007, 0.800000012, 1.39999998, 1]},{"id":11,"in":[],"index":1,"name":"CellColor","op":"paramcolor","pos":[40, 1100],"text":"Cell Color","value":[0.00499999989, 0.0149999997, 0.0250000004, 1]},{"id":12,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0],[11, 0]],"name":"Hex Grid","op":"custom","outs":[["Resultat",4]],"pins":[["UV",2],["CyCanvasSize",4],["Scale",1],["LineWidth",1],["Softness",1],["NodeSize",1],["NodeIntensity",1],["LineOpacity",1],["CellOpacity",1],["LineColor",3],["CellColor",3]],"pos":[420, 40],"text":"    float2 p = UV * max(Scale, 1.0);\n    p.x *= 1.1547005;\n    float2 a = frac(p) - 0.5;\n    float2 b = frac(p + 0.5) - 0.5;\n    float2 g = dot(a, a) < dot(b, b) ? a : b;\n    float h = max(abs(g.y), abs(g.x) * 0.866025 + abs(g.y) * 0.5);\n    float lineMask = 1.0 - smoothstep(LineWidth, LineWidth + Softness, abs(0.5 - h));\n    float node = 1.0 - smoothstep(NodeSize, NodeSize + Softness, length(g));\n    float m = saturate(max(lineMask, node * NodeIntensity));\n    return float4(lerp(CellColor.rgb, LineColor.rgb, m), lerp(CellOpacity, LineOpacity, m));","value":[0, 0, 0, 0]},{"id":13,"in":[[12, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@generator
//@halo 0
//@param Scale float 12 1 256 "Scale"
//@param LineWidth float 0.025 0.001 0.25 "Line Width"
//@param Softness float 0.008 0.0001 0.1 "Softness"
//@param NodeSize float 0.055 0.001 0.3 "Node Size"
//@param NodeIntensity float 0.5 0 1 "Node Intensity"
//@param LineOpacity float 1 0 1 "Line Opacity"
//@param CellOpacity float 1 0 1 "Cell Opacity"
//@param LineColor color 0.05 0.8 1.4 1 "Line Color"
//@param CellColor color 0.005 0.015 0.025 1 "Cell Color"
// Imported preset (Draw / Pattern): Hexagonal technical grid with independently controlled cells, borders, and corner nodes.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Scale, float LineWidth, float Softness, float NodeSize, float NodeIntensity, float LineOpacity, float CellOpacity, float3 LineColor, float3 CellColor)
{
    float2 p = UV * max(Scale, 1.0);
    p.x *= 1.1547005;
    float2 a = frac(p) - 0.5;
    float2 b = frac(p + 0.5) - 0.5;
    float2 g = dot(a, a) < dot(b, b) ? a : b;
    float h = max(abs(g.y), abs(g.x) * 0.866025 + abs(g.y) * 0.5);
    float lineMask = 1.0 - smoothstep(LineWidth, LineWidth + Softness, abs(0.5 - h));
    float node = 1.0 - smoothstep(NodeSize, NodeSize + Softness, length(g));
    float m = saturate(max(lineMask, node * NodeIntensity));
    return float4(lerp(CellColor.rgb, LineColor.rgb, m), lerp(CellOpacity, LineOpacity, m));
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Scale, LineWidth, Softness, NodeSize, NodeIntensity, LineOpacity, CellOpacity, LineColor.rgb, CellColor.rgb);
}
