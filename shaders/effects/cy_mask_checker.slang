//@name Checker
//@category Masques
//@graph {"category":"Masques","inputs":0,"name":"Checker","nextId":7,"nodes":[{"id":1,"in":[],"op":"uv","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"canvassize","pos":[40, 130],"value":[0, 0, 0, 0]},{"id":3,"in":[],"index":1,"max":512,"min":1,"name":"Tiling","op":"param","pos":[40, 220],"text":"Tiling","value":[8, 0, 0, 0]},{"id":4,"in":[],"index":1,"name":"Invert","op":"param","pos":[40, 330],"text":"Invert","value":[0, 0, 0, 0]},{"id":5,"in":[[1, 0],[2, 0],[3, 0],[4, 0]],"name":"Checker","op":"custom","outs":[["Resultat",4]],"pins":[["UV",2],["CyCanvasSize",4],["Tiling",1],["Invert",1]],"pos":[420, 40],"text":"    float2 c = floor(UV * max(Tiling, 1.0));\n    float v = fmod(c.x + c.y, 2.0);\n    v = lerp(v, 1.0 - v, saturate(Invert));\n    return float4(v.xxx, 1);","value":[0, 0, 0, 0]},{"id":6,"in":[[5, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@generator
//@halo 0
//@param Tiling float 8 1 512 "Tiling"
//@param Invert float 0 0 1 "Invert"
// Imported preset (Mask / Mask): Procedural checker mask.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Tiling, float Invert)
{
    float2 c = floor(UV * max(Tiling, 1.0));
    float v = fmod(c.x + c.y, 2.0);
    v = lerp(v, 1.0 - v, saturate(Invert));
    return float4(v.xxx, 1);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Tiling, Invert);
}
