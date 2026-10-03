//@name LUT
//@category Traitements
//@graph {"name":"LUT","category":"Traitements","inputs":2,"premultiplied":false,"nextId":10,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"texture","pos":[40,150],"in":[],"value":[0,0,0,0],"index":1,"name":"LUTTexture"},{"id":3,"op":"uv","pos":[40,260],"in":[],"value":[0,0,0,0]},{"id":4,"op":"canvassize","pos":[40,350],"in":[],"value":[0,0,0,0]},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[16.0,0.0,0.0,0.0],"name":"LUTSize","text":"LUT Size","index":1,"min":2.0,"max":64.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"FlipVertical","text":"Flip Vertical","index":1},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Strength","text":"Strength","index":1},{"id":8,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0]],"value":[0,0,0,0],"name":"LUT","text":"    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float n=max(LUTSize,2.0); float3 c=saturate(s.rgb); c.g=lerp(c.g,1.0-c.g,saturate(FlipVertical)); float z=c.b*(n-1.0); float z0=floor(z); float z1=min(z0+1.0,n-1.0); float2 uv0=float2((z0*n+c.r*(n-1.0)+0.5)/(n*n),(c.g*(n-1.0)+0.5)/n); float2 uv1=float2((z1*n+c.r*(n-1.0)+0.5)/(n*n),(c.g*(n-1.0)+0.5)/n); float3 graded=lerp(Texture2DSample(LUTTexture,LUTTextureSampler,uv0).rgb,Texture2DSample(LUTTexture,LUTTextureSampler,uv1).rgb,frac(z)); return float4(lerp(s.rgb,graded,saturate(Strength)),s.a);","pins":[["CyInput",5],["LUTTexture",5],["UV",2],["CyCanvasSize",4],["LUTSize",1],["FlipVertical",1],["Strength",1]],"outs":[["Resultat",4]]},{"id":9,"op":"output","pos":[780,40],"in":[[8,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@input LUT
//@halo full
//@param LUTSize float 16 2 64 "LUT Size"
//@param FlipVertical float 0 0 1 "Flip Vertical"
//@param Strength float 0 0 1 "Strength"

// Imported preset (Effect / Color & Tone): Applies a standard horizontal 2D strip LUT (N*N by N) with adjustable strength.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
static const uint LUTTexture = 1;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float LUTSize, float FlipVertical, float Strength)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float n=max(LUTSize,2.0); float3 c=saturate(s.rgb); c.g=lerp(c.g,1.0-c.g,saturate(FlipVertical)); float z=c.b*(n-1.0); float z0=floor(z); float z1=min(z0+1.0,n-1.0); float2 uv0=float2((z0*n+c.r*(n-1.0)+0.5)/(n*n),(c.g*(n-1.0)+0.5)/n); float2 uv1=float2((z1*n+c.r*(n-1.0)+0.5)/(n*n),(c.g*(n-1.0)+0.5)/n); float3 graded=lerp(Texture2DSample(LUTTexture,LUTTextureSampler,uv0).rgb,Texture2DSample(LUTTexture,LUTTextureSampler,uv1).rgb,frac(z)); return float4(lerp(s.rgb,graded,saturate(Strength)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), LUTSize, FlipVertical, Strength);
}
