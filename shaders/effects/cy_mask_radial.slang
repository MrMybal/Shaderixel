//@name Radial
//@category Masques
//@graph {"category":"Masques","inputs":0,"name":"Radial","nextId":10,"nodes":[{"id":1,"in":[],"op":"uv","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"canvassize","pos":[40, 130],"value":[0, 0, 0, 0]},{"id":3,"in":[],"index":1,"max":2,"min":0.00100000005,"name":"Radius","op":"param","pos":[40, 220],"text":"Radius","value":[0.349999994, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":1,"min":9.99999975e-05,"name":"Softness","op":"param","pos":[40, 330],"text":"Softness","value":[0.100000001, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":8,"min":0.00999999978,"name":"Aspect","op":"param","pos":[40, 440],"text":"Aspect","value":[1, 0, 0, 0]},{"id":6,"in":[],"index":1,"name":"Invert","op":"param","pos":[40, 550],"text":"Invert","value":[0, 0, 0, 0]},{"id":7,"in":[],"index":1,"name":"Center","op":"param2","pos":[40, 660],"text":"Center","value":[0.5, 0.5, 0, 0]},{"id":8,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0]],"name":"Radial","op":"custom","outs":[["Resultat",4]],"pins":[["UV",2],["CyCanvasSize",4],["Radius",1],["Softness",1],["Aspect",1],["Invert",1],["Center",3]],"pos":[420, 40],"text":"    float2 p = UV - Center.xy;\n    p.x *= Aspect;\n    float d = length(p);\n    float v = 1.0 - smoothstep(Radius - Softness, Radius + Softness, d);\n    v = lerp(v, 1.0 - v, saturate(Invert));\n    return float4(v.xxx, 1);","value":[0, 0, 0, 0]},{"id":9,"in":[[8, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@generator
//@halo 0
//@param Radius float 0.35 0.001 2 "Radius"
//@param Softness float 0.1 0.0001 1 "Softness"
//@param Aspect float 1 0.01 8 "Aspect"
//@param Invert float 0 0 1 "Invert"
//@param Center float2 0.5 0.5 0 1 "Center"
// Imported preset (Mask / Mask): Configurable radial mask independent of source color.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float Softness, float Aspect, float Invert, float3 Center)
{
    float2 p = UV - Center.xy;
    p.x *= Aspect;
    float d = length(p);
    float v = 1.0 - smoothstep(Radius - Softness, Radius + Softness, d);
    v = lerp(v, 1.0 - v, saturate(Invert));
    return float4(v.xxx, 1);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, Softness, Aspect, Invert, float3(Center, 0.0));
}
