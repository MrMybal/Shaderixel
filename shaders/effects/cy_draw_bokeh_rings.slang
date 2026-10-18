//@name Bokeh Rings
//@category Dessin
//@graph {"category":"Dessin","inputs":0,"name":"Bokeh Rings","nextId":15,"nodes":[{"id":1,"in":[],"op":"uv","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"canvassize","pos":[40, 130],"value":[0, 0, 0, 0]},{"id":3,"in":[],"index":1,"max":8,"min":0.100000001,"name":"Aspect","op":"param","pos":[40, 220],"text":"Aspect","value":[1, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":2,"min":0.00100000005,"name":"Radius","op":"param","pos":[40, 330],"text":"Radius","value":[0.219999999, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":0.5,"min":0.00100000005,"name":"RingWidth","op":"param","pos":[40, 440],"text":"Ring Width","value":[0.0250000004, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":16,"min":0.100000001,"name":"Softness","op":"param","pos":[40, 550],"text":"Softness","value":[2, 0, 0, 0]},{"id":7,"in":[],"index":1,"max":64,"min":0,"name":"Intensity","op":"param","pos":[40, 660],"text":"Intensity","value":[2, 0, 0, 0]},{"id":8,"in":[],"index":1,"name":"Opacity","op":"param","pos":[40, 770],"text":"Opacity","value":[1, 0, 0, 0]},{"id":9,"in":[],"index":1,"name":"Center","op":"param2","pos":[40, 880],"text":"Center","value":[0.5, 0.5, 0, 0]},{"id":10,"in":[],"index":1,"name":"Offset","op":"param2","pos":[40, 990],"text":"Offset","value":[0.200000003, 0.0700000003, 0, 0]},{"id":11,"in":[],"index":1,"name":"ColorA","op":"paramcolor","pos":[40, 1100],"text":"Color A","value":[1, 0.449999988, 0.119999997, 1]},{"id":12,"in":[],"index":1,"name":"ColorB","op":"paramcolor","pos":[40, 1210],"text":"Color B","value":[0.100000001, 0.400000006, 1, 1]},{"id":13,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0],[11, 0],[12, 0]],"name":"Bokeh Rings","op":"custom","outs":[["Resultat",4]],"pins":[["UV",2],["CyCanvasSize",4],["Aspect",1],["Radius",1],["RingWidth",1],["Softness",1],["Intensity",1],["Opacity",1],["Center",3],["Offset",3],["ColorA",3],["ColorB",3]],"pos":[420, 40],"text":"    float2 p = UV - Center.xy;\n    p.x *= Aspect;\n    float ring1 = pow(saturate(1.0 - abs(length(p) - Radius) / max(RingWidth, 0.0001)), Softness);\n    float ring2 = pow(saturate(1.0 - abs(length(p - Offset.xy) - Radius * 0.55) / max(RingWidth * 0.7, 0.0001)), Softness);\n    float ring3 = pow(saturate(1.0 - abs(length(p + Offset.xy * 1.4) - Radius * 0.35) / max(RingWidth * 0.55, 0.0001)), Softness);\n    float3 rgb = ColorA.rgb * ring1 + ColorB.rgb * (ring2 + ring3);\n    float a = saturate((ring1 + ring2 + ring3) * Opacity);\n    return float4(rgb * Intensity, a);","value":[0, 0, 0, 0]},{"id":14,"in":[[13, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@generator
//@halo 0
//@param Aspect float 1 0.1 8 "Aspect"
//@param Radius float 0.22 0.001 2 "Radius"
//@param RingWidth float 0.025 0.001 0.5 "Ring Width"
//@param Softness float 2 0.1 16 "Softness"
//@param Intensity float 2 0 64 "Intensity"
//@param Opacity float 1 0 1 "Opacity"
//@param Center float2 0.5 0.5 0 1 "Center"
//@param Offset float2 0.2 0.07 0 1 "Offset"
//@param ColorA color 1 0.45 0.12 1 "Color A"
//@param ColorB color 0.1 0.4 1 1 "Color B"
// Imported preset (Draw / Light & Space): Procedural bokeh ring cluster for flare and depth-of-field overlays.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Aspect, float Radius, float RingWidth, float Softness, float Intensity, float Opacity, float3 Center, float3 Offset, float3 ColorA, float3 ColorB)
{
    float2 p = UV - Center.xy;
    p.x *= Aspect;
    float ring1 = pow(saturate(1.0 - abs(length(p) - Radius) / max(RingWidth, 0.0001)), Softness);
    float ring2 = pow(saturate(1.0 - abs(length(p - Offset.xy) - Radius * 0.55) / max(RingWidth * 0.7, 0.0001)), Softness);
    float ring3 = pow(saturate(1.0 - abs(length(p + Offset.xy * 1.4) - Radius * 0.35) / max(RingWidth * 0.55, 0.0001)), Softness);
    float3 rgb = ColorA.rgb * ring1 + ColorB.rgb * (ring2 + ring3);
    float a = saturate((ring1 + ring2 + ring3) * Opacity);
    return float4(rgb * Intensity, a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Aspect, Radius, RingWidth, Softness, Intensity, Opacity, float3(Center, 0.0), float3(Offset, 0.0), ColorA.rgb, ColorB.rgb);
}
