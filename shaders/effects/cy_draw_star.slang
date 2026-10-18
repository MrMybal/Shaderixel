//@name Star
//@category Dessin
//@graph {"category":"Dessin","inputs":0,"name":"Star","nextId":15,"nodes":[{"id":1,"in":[],"op":"uv","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"canvassize","pos":[40, 130],"value":[0, 0, 0, 0]},{"id":3,"in":[],"index":1,"max":64,"min":2,"name":"Points","op":"param","pos":[40, 220],"text":"Points","value":[5, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":2,"min":0.00100000005,"name":"InnerRadius","op":"param","pos":[40, 330],"text":"Inner Radius","value":[0.119999997, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":2,"min":0.00100000005,"name":"OuterRadius","op":"param","pos":[40, 440],"text":"Outer Radius","value":[0.319999993, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":8,"min":0.0500000007,"name":"PointSharpness","op":"param","pos":[40, 550],"text":"Point Sharpness","value":[1, 0, 0, 0]},{"id":7,"in":[],"index":1,"max":6.28299999,"min":-6.28299999,"name":"Rotation","op":"param","pos":[40, 660],"text":"Rotation","value":[0, 0, 0, 0]},{"id":8,"in":[],"index":1,"max":8,"min":0.00999999978,"name":"Aspect","op":"param","pos":[40, 770],"text":"Aspect","value":[1, 0, 0, 0]},{"id":9,"in":[],"index":1,"max":0.100000001,"min":9.99999975e-05,"name":"Softness","op":"param","pos":[40, 880],"text":"Softness","value":[0.00400000019, 0, 0, 0]},{"id":10,"in":[],"index":1,"name":"Opacity","op":"param","pos":[40, 990],"text":"Opacity","value":[1, 0, 0, 0]},{"id":11,"in":[],"index":1,"name":"Center","op":"param2","pos":[40, 1100],"text":"Center","value":[0.5, 0.5, 0, 0]},{"id":12,"in":[],"index":1,"name":"Color","op":"paramcolor","pos":[40, 1210],"text":"Color","value":[1, 1, 1, 1]},{"id":13,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0],[11, 0],[12, 0]],"name":"Star","op":"custom","outs":[["Resultat",4]],"pins":[["UV",2],["CyCanvasSize",4],["Points",1],["InnerRadius",1],["OuterRadius",1],["PointSharpness",1],["Rotation",1],["Aspect",1],["Softness",1],["Opacity",1],["Center",3],["Color",3]],"pos":[420, 40],"text":"    float2 p = UV - Center.xy;\n    p.x *= Aspect;\n    float a = atan2(p.y, p.x) + Rotation;\n    float count = max(Points, 2.0);\n    float wave = 0.5 + 0.5 * cos(a * count);\n    float r = lerp(InnerRadius, OuterRadius, pow(wave, max(PointSharpness, 0.05)));\n    float mask = 1.0 - smoothstep(r, r + Softness, length(p));\n    return float4(Color.rgb, mask * Opacity);","value":[0, 0, 0, 0]},{"id":14,"in":[[13, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@generator
//@halo 0
//@param Points float 5 2 64 "Points"
//@param InnerRadius float 0.12 0.001 2 "Inner Radius"
//@param OuterRadius float 0.32 0.001 2 "Outer Radius"
//@param PointSharpness float 1 0.05 8 "Point Sharpness"
//@param Rotation float 0 -6.283 6.283 "Rotation"
//@param Aspect float 1 0.01 8 "Aspect"
//@param Softness float 0.004 0.0001 0.1 "Softness"
//@param Opacity float 1 0 1 "Opacity"
//@param Center float2 0.5 0.5 0 1 "Center"
//@param Color color 1 1 1 1 "Color"
// Imported preset (Draw / Shape & Gradient): Procedural star or flower shape with controllable point count.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Points, float InnerRadius, float OuterRadius, float PointSharpness, float Rotation, float Aspect, float Softness, float Opacity, float3 Center, float3 Color)
{
    float2 p = UV - Center.xy;
    p.x *= Aspect;
    float a = atan2(p.y, p.x) + Rotation;
    float count = max(Points, 2.0);
    float wave = 0.5 + 0.5 * cos(a * count);
    float r = lerp(InnerRadius, OuterRadius, pow(wave, max(PointSharpness, 0.05)));
    float mask = 1.0 - smoothstep(r, r + Softness, length(p));
    return float4(Color.rgb, mask * Opacity);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Points, InnerRadius, OuterRadius, PointSharpness, Rotation, Aspect, Softness, Opacity, float3(Center, 0.0), Color.rgb);
}
