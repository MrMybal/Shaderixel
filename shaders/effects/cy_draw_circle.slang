//@name Circle
//@category Dessin
//@graph {"category":"Dessin","inputs":0,"name":"Circle","nextId":13,"nodes":[{"id":1,"in":[],"op":"uv","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"canvassize","pos":[40, 130],"value":[0, 0, 0, 0]},{"id":3,"in":[],"index":1,"max":2,"min":0.00100000005,"name":"Radius","op":"param","pos":[40, 220],"text":"Radius","value":[0.300000012, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":1,"min":0.00100000005,"name":"Thickness","op":"param","pos":[40, 330],"text":"Thickness","value":[0.0399999991, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":0.25,"min":9.99999975e-05,"name":"Softness","op":"param","pos":[40, 440],"text":"Softness","value":[0.00499999989, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":8,"min":0.00999999978,"name":"Aspect","op":"param","pos":[40, 550],"text":"Aspect","value":[1, 0, 0, 0]},{"id":7,"in":[],"index":1,"name":"Outline","op":"param","pos":[40, 660],"text":"Outline","value":[0, 0, 0, 0]},{"id":8,"in":[],"index":1,"name":"Opacity","op":"param","pos":[40, 770],"text":"Opacity","value":[1, 0, 0, 0]},{"id":9,"in":[],"index":1,"name":"Center","op":"param2","pos":[40, 880],"text":"Center","value":[0.5, 0.5, 0, 0]},{"id":10,"in":[],"index":1,"name":"Color","op":"paramcolor","pos":[40, 990],"text":"Color","value":[1, 1, 1, 1]},{"id":11,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0]],"name":"Circle","op":"custom","outs":[["Resultat",4]],"pins":[["UV",2],["CyCanvasSize",4],["Radius",1],["Thickness",1],["Softness",1],["Aspect",1],["Outline",1],["Opacity",1],["Center",3],["Color",3]],"pos":[420, 40],"text":"    float2 p = UV - Center.xy;\n    p.x *= Aspect;\n    float d = length(p);\n    float outer = 1.0 - smoothstep(Radius, Radius + Softness, d);\n    float inner = 1.0 - smoothstep(max(Radius - Thickness, 0.0), max(Radius - Thickness, 0.0) + Softness, d);\n    float a = lerp(outer, saturate(outer - inner), saturate(Outline));\n    return float4(Color.rgb, a * Opacity);","value":[0, 0, 0, 0]},{"id":12,"in":[[11, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@generator
//@halo 0
//@param Radius float 0.3 0.001 2 "Radius"
//@param Thickness float 0.04 0.001 1 "Thickness"
//@param Softness float 0.005 0.0001 0.25 "Softness"
//@param Aspect float 1 0.01 8 "Aspect"
//@param Outline float 0 0 1 "Outline"
//@param Opacity float 1 0 1 "Opacity"
//@param Center float2 0.5 0.5 0 1 "Center"
//@param Color color 1 1 1 1 "Color"
// Imported preset (Draw / Shape & Gradient): Antialiased filled or outlined circle with HDR color.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float Thickness, float Softness, float Aspect, float Outline, float Opacity, float3 Center, float3 Color)
{
    float2 p = UV - Center.xy;
    p.x *= Aspect;
    float d = length(p);
    float outer = 1.0 - smoothstep(Radius, Radius + Softness, d);
    float inner = 1.0 - smoothstep(max(Radius - Thickness, 0.0), max(Radius - Thickness, 0.0) + Softness, d);
    float a = lerp(outer, saturate(outer - inner), saturate(Outline));
    return float4(Color.rgb, a * Opacity);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, Thickness, Softness, Aspect, Outline, Opacity, float3(Center, 0.0), Color.rgb);
}
