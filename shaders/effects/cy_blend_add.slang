//@name Add
//@category Fusions
//@graph {"name":"Add","category":"Fusions","inputs":3,"premultiplied":false,"nextId":9,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyBackground"},{"id":2,"op":"texture","pos":[40,150],"in":[],"value":[0,0,0,0],"index":1,"name":"CyForeground"},{"id":3,"op":"texture","pos":[40,260],"in":[],"value":[0,0,0,0],"index":2,"name":"CyMask"},{"id":4,"op":"uv","pos":[40,370],"in":[],"value":[0,0,0,0]},{"id":5,"op":"canvassize","pos":[40,460],"in":[],"value":[0,0,0,0]},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Opacity","text":"Opacity","index":1},{"id":7,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0]],"value":[0,0,0,0],"name":"Add","text":"    float4 bg=Texture2DSample(CyBackground,CyBackgroundSampler,UV); float4 fg=Texture2DSample(CyForeground,CyForegroundSampler,UV); float m=Texture2DSample(CyMask,CyMaskSampler,UV).r; float3 b=bg.rgb+fg.rgb; float a=saturate(fg.a*m*Opacity); return float4(lerp(bg.rgb,b,a),a+bg.a*(1.0-a));","pins":[["CyBackground",5],["CyForeground",5],["CyMask",5],["UV",2],["CyCanvasSize",4],["Opacity",1]],"outs":[["Resultat",4]]},{"id":8,"op":"output","pos":[780,40],"in":[[7,0],[0,0],[0,0]],"value":[0,0,0,0]}],"kind":1}
//@blend
//@input Fond
//@input Calque
//@input Masque
//@halo 0
//@param Opacity float 1 0 1 "Opacity"

// Imported preset (Blend / Compositing): Additive HDR blend.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyBackground = 0;
static const uint CyForeground = 1;
static const uint CyMask = 2;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Opacity)
{
    float4 bg=Texture2DSample(CyBackground,CyBackgroundSampler,UV); float4 fg=Texture2DSample(CyForeground,CyForegroundSampler,UV); float m=Texture2DSample(CyMask,CyMaskSampler,UV).r; float3 b=bg.rgb+fg.rgb; float a=saturate(fg.a*m*Opacity); return float4(lerp(bg.rgb,b,a),a+bg.a*(1.0-a));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Opacity);
}
