//@name Seamless Offset
//@category Traitements
//@graph {"name":"Seamless Offset","category":"Traitements","inputs":1,"premultiplied":false,"nextId":9,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.0799999982,0.0,0.0,0.0],"name":"Feather","text":"Feather","index":1,"min":0.00100000005,"max":0.5},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Strength","text":"Strength","index":1},{"id":6,"op":"param2","pos":[40,550],"in":[],"value":[0.5,0.5,0.0,0.0],"name":"Offset","text":"Offset","index":1},{"id":7,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0]],"value":[0,0,0,0],"name":"Seamless Offset","text":"    float2 uv=frac(UV+Offset.xy);float4 base=Texture2DSample(CyInput,CyInputSampler,uv);float2 edge=min(uv,1.0-uv);float wx=1.0-smoothstep(0.0,Feather,edge.x);float wy=1.0-smoothstep(0.0,Feather,edge.y);float4 x=Texture2DSample(CyInput,CyInputSampler,frac(uv+float2(.5,0)));float4 y=Texture2DSample(CyInput,CyInputSampler,frac(uv+float2(0,.5)));float4 xy=Texture2DSample(CyInput,CyInputSampler,frac(uv+.5));float4 blended=lerp(lerp(base,x,wx),lerp(y,xy,wx),wy);return lerp(base,blended,Strength);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Feather",1],["Strength",1],["Offset",3]],"outs":[["Resultat",4]]},{"id":8,"op":"output","pos":[780,40],"in":[[7,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo full
//@param Feather float 0.08 0.001 0.5 "Feather"
//@param Strength float 1 0 1 "Strength"
//@param Offset float2 0.5 0.5 0 1 "Offset"

// Imported preset (Effect / PBR): Wraps and crossfades an offset texture to reduce visible borders during seamless texture authoring.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Feather, float Strength, float3 Offset)
{
    float2 uv=frac(UV+Offset.xy);float4 base=Texture2DSample(CyInput,CyInputSampler,uv);float2 edge=min(uv,1.0-uv);float wx=1.0-smoothstep(0.0,Feather,edge.x);float wy=1.0-smoothstep(0.0,Feather,edge.y);float4 x=Texture2DSample(CyInput,CyInputSampler,frac(uv+float2(.5,0)));float4 y=Texture2DSample(CyInput,CyInputSampler,frac(uv+float2(0,.5)));float4 xy=Texture2DSample(CyInput,CyInputSampler,frac(uv+.5));float4 blended=lerp(lerp(base,x,wx),lerp(y,xy,wx),wy);return lerp(base,blended,Strength);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Feather, Strength, float3(Offset, 0.0));
}
