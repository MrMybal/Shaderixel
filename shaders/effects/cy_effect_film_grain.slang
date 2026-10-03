//@name Film Grain
//@category Traitements
//@graph {"name":"Film Grain","category":"Traitements","inputs":1,"premultiplied":false,"nextId":9,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.0399999991,0.0,0.0,0.0],"name":"Amount","text":"Amount","index":1},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Seed","text":"Seed","index":1,"min":-10000.0,"max":10000.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Colored","text":"Colored","index":1},{"id":7,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0]],"value":[0,0,0,0],"name":"Film Grain","text":"    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float2 p=floor(UV*CyCanvasSize.xy)+Seed; float r=frac(sin(dot(p,float2(12.9898,78.233)))*43758.5453)-0.5; float g=frac(sin(dot(p,float2(39.346,11.135)))*24634.6345)-0.5; float b=frac(sin(dot(p,float2(73.156,52.235)))*56445.2345)-0.5; float3 n=lerp(r.xxx,float3(r,g,b),saturate(Colored)); return float4(s.rgb+n*Amount,s.a);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Amount",1],["Seed",1],["Colored",1]],"outs":[["Resultat",4]]},{"id":8,"op":"output","pos":[780,40],"in":[[7,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo 0
//@param Amount float 0.04 0 1 "Amount"
//@param Seed float 0 -10000 10000 "Seed"
//@param Colored float 0 0 1 "Colored"

// Imported preset (Effect / Stylize): Resolution-aware monochrome or colored film grain.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Amount, float Seed, float Colored)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float2 p=floor(UV*CyCanvasSize.xy)+Seed; float r=frac(sin(dot(p,float2(12.9898,78.233)))*43758.5453)-0.5; float g=frac(sin(dot(p,float2(39.346,11.135)))*24634.6345)-0.5; float b=frac(sin(dot(p,float2(73.156,52.235)))*56445.2345)-0.5; float3 n=lerp(r.xxx,float3(r,g,b),saturate(Colored)); return float4(s.rgb+n*Amount,s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Amount, Seed, Colored);
}
