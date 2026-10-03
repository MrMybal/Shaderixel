//@name Dither
//@category Traitements
//@graph {"name":"Dither","category":"Traitements","inputs":1,"premultiplied":false,"nextId":8,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[16.0,0.0,0.0,0.0],"name":"Levels","text":"Levels","index":1,"min":2.0,"max":256.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Amount","text":"Amount","index":1,"min":0.0,"max":4.0},{"id":6,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0]],"value":[0,0,0,0],"name":"Dither","text":"    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float2 p=floor(UV*CyCanvasSize.xy); float n=frac(sin(dot(p,float2(12.9898,78.233)))*43758.5453)-0.5; float levels=max(Levels,2.0); float3 v=floor(saturate(s.rgb+n*Amount/levels)*levels)/max(levels-1.0,1.0); return float4(v,s.a);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Levels",1],["Amount",1]],"outs":[["Resultat",4]]},{"id":7,"op":"output","pos":[780,40],"in":[[6,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo 0
//@param Levels float 16 2 256 "Levels"
//@param Amount float 1 0 4 "Amount"

// Imported preset (Effect / Stylize): Ordered-style procedural dithering and bit-depth reduction.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Levels, float Amount)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float2 p=floor(UV*CyCanvasSize.xy); float n=frac(sin(dot(p,float2(12.9898,78.233)))*43758.5453)-0.5; float levels=max(Levels,2.0); float3 v=floor(saturate(s.rgb+n*Amount/levels)*levels)/max(levels-1.0,1.0); return float4(v,s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Levels, Amount);
}
