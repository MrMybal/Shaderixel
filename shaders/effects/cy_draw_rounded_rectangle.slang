//@name Rounded Rectangle
//@category Dessin
//@graph {"category":"Dessin","inputs":0,"name":"Rounded Rectangle","nextId":13,"nodes":[{"id":1,"in":[],"op":"uv","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"canvassize","pos":[40, 130],"value":[0, 0, 0, 0]},{"id":3,"in":[],"index":1,"max":0.5,"min":0,"name":"CornerRadius","op":"param","pos":[40, 220],"text":"Corner Radius","value":[0.0500000007, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":0.5,"min":0.00100000005,"name":"BorderWidth","op":"param","pos":[40, 330],"text":"Border Width","value":[0.0199999996, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":0.100000001,"min":9.99999975e-05,"name":"Softness","op":"param","pos":[40, 440],"text":"Softness","value":[0.00300000003, 0, 0, 0]},{"id":6,"in":[],"index":1,"name":"Outline","op":"param","pos":[40, 550],"text":"Outline","value":[0, 0, 0, 0]},{"id":7,"in":[],"index":1,"name":"Opacity","op":"param","pos":[40, 660],"text":"Opacity","value":[1, 0, 0, 0]},{"id":8,"in":[],"index":1,"name":"Center","op":"param2","pos":[40, 770],"text":"Center","value":[0.5, 0.5, 0, 0]},{"id":9,"in":[],"index":1,"name":"HalfSize","op":"param2","pos":[40, 880],"text":"Half Size","value":[0.300000012, 0.200000003, 0, 0]},{"id":10,"in":[],"index":1,"name":"Color","op":"paramcolor","pos":[40, 990],"text":"Color","value":[1, 1, 1, 1]},{"id":11,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0]],"name":"Rounded Rectangle","op":"custom","outs":[["Resultat",4]],"pins":[["UV",2],["CyCanvasSize",4],["CornerRadius",1],["BorderWidth",1],["Softness",1],["Outline",1],["Opacity",1],["Center",3],["HalfSize",3],["Color",3]],"pos":[420, 40],"text":"    float2 p = abs(UV - Center.xy) - HalfSize.xy + CornerRadius;\n    float d = min(max(p.x, p.y), 0.0) + length(max(p, 0.0)) - CornerRadius;\n    float fill = 1.0 - smoothstep(0.0, Softness, d);\n    float inside = 1.0 - smoothstep(-BorderWidth, -BorderWidth + Softness, d);\n    float a = lerp(fill, saturate(fill - inside), saturate(Outline));\n    return float4(Color.rgb, a * Opacity);","value":[0, 0, 0, 0]},{"id":12,"in":[[11, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@generator
//@halo 0
//@param CornerRadius float 0.05 0 0.5 "Corner Radius"
//@param BorderWidth float 0.02 0.001 0.5 "Border Width"
//@param Softness float 0.003 0.0001 0.1 "Softness"
//@param Outline float 0 0 1 "Outline"
//@param Opacity float 1 0 1 "Opacity"
//@param Center float2 0.5 0.5 0 1 "Center"
//@param HalfSize float2 0.3 0.2 0 1 "Half Size"
//@param Color color 1 1 1 1 "Color"
// Imported preset (Draw / Shape & Gradient): Signed-distance rounded rectangle with border support.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
float4 CyPreset(float2 UV, float4 CyCanvasSize, float CornerRadius, float BorderWidth, float Softness, float Outline, float Opacity, float3 Center, float3 HalfSize, float3 Color)
{
    float2 p = abs(UV - Center.xy) - HalfSize.xy + CornerRadius;
    float d = min(max(p.x, p.y), 0.0) + length(max(p, 0.0)) - CornerRadius;
    float fill = 1.0 - smoothstep(0.0, Softness, d);
    float inside = 1.0 - smoothstep(-BorderWidth, -BorderWidth + Softness, d);
    float a = lerp(fill, saturate(fill - inside), saturate(Outline));
    return float4(Color.rgb, a * Opacity);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), CornerRadius, BorderWidth, Softness, Outline, Opacity, float3(Center, 0.0), float3(HalfSize, 0.0), Color.rgb);
}
