//@name Checker
//@category Masques
//@graph {"name":"Checker","category":"Masques","inputs":0,"premultiplied":false,"nextId":7,"nodes":[{"id":1,"op":"uv","pos":[40,40],"in":[],"value":[0,0,0,0]},{"id":2,"op":"canvassize","pos":[40,130],"in":[],"value":[0,0,0,0]},{"id":3,"op":"param","pos":[40,220],"in":[],"value":[8.0,0.0,0.0,0.0],"name":"Tiling","text":"Tiling","index":1,"min":1.0,"max":512.0},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Invert","text":"Invert","index":1},{"id":5,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0]],"value":[0,0,0,0],"name":"Checker","text":"    float2 c=floor(UV*max(Tiling,1.0)); float v=fmod(c.x+c.y,2.0); v=lerp(v,1.0-v,saturate(Invert)); return float4(v.xxx,1);","pins":[["UV",2],["CyCanvasSize",4],["Tiling",1],["Invert",1]],"outs":[["Resultat",4]]},{"id":6,"op":"output","pos":[780,40],"in":[[5,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@generator
//@halo 0
//@param Tiling float 8 1 512 "Tiling"
//@param Invert float 0 0 1 "Invert"

// Imported preset (Mask / Mask): Procedural checker mask.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Tiling, float Invert)
{
    float2 c=floor(UV*max(Tiling,1.0)); float v=fmod(c.x+c.y,2.0); v=lerp(v,1.0-v,saturate(Invert)); return float4(v.xxx,1);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Tiling, Invert);
}
