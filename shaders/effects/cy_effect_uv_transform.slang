//@name UV Transform
//@category Traitements
//@graph {"name":"UV Transform","category":"Traitements","inputs":1,"premultiplied":false,"nextId":10,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Rotation","text":"Rotation","index":1,"min":-6.28299999,"max":6.28299999},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Scale","text":"Scale","index":1,"min":0.00999999978,"max":16.0},{"id":6,"op":"param2","pos":[40,550],"in":[],"value":[0.5,0.5,0.0,0.0],"name":"Pivot","text":"Pivot","index":1},{"id":7,"op":"param2","pos":[40,660],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Offset","text":"Offset","index":1},{"id":8,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0]],"value":[0,0,0,0],"name":"UV Transform","text":"    float2 p=UV-Pivot.xy-Offset.xy; float c=cos(Rotation); float s=sin(Rotation); p=mul(float2x2(c,-s,s,c),p)/max(Scale,0.0001); float2 q=Pivot.xy+p; return Texture2DSample(CyInput,CyInputSampler,q);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Rotation",1],["Scale",1],["Pivot",3],["Offset",3]],"outs":[["Resultat",4]]},{"id":9,"op":"output","pos":[780,40],"in":[[8,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo full
//@param Rotation float 0 -6.283 6.283 "Rotation"
//@param Scale float 1 0.01 16 "Scale"
//@param Pivot float2 0.5 0.5 0 1 "Pivot"
//@param Offset float2 0 0 0 1 "Offset"

// Imported preset (Effect / Distort): Full-screen translate, rotate, scale, and pivot transform.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Rotation, float Scale, float3 Pivot, float3 Offset)
{
    float2 p=UV-Pivot.xy-Offset.xy; float c=cos(Rotation); float s=sin(Rotation); p=mul(float2x2(c,-s,s,c),p)/max(Scale,0.0001); float2 q=Pivot.xy+p; return Texture2DSample(CyInput,CyInputSampler,q);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Rotation, Scale, float3(Pivot, 0.0), float3(Offset, 0.0));
}
