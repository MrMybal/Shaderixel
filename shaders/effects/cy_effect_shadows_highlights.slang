//@name Shadows Highlights
//@category Traitements
//@graph {"name":"Shadows Highlights","category":"Traitements","inputs":1,"premultiplied":false,"nextId":10,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.25,0.0,0.0,0.0],"name":"ShadowAmount","text":"Shadow Amount","index":1,"min":0.0,"max":2.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.25,0.0,0.0,0.0],"name":"HighlightAmount","text":"Highlight Amount","index":1,"min":0.0,"max":2.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[2.0,0.0,0.0,0.0],"name":"Radius","text":"Radius","index":1,"min":0.00999999978,"max":8.0},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Strength","text":"Strength","index":1},{"id":8,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0]],"value":[0,0,0,0],"name":"Shadows Highlights","text":"    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float l=max(dot(s.rgb,float3(.2126,.7152,.0722)),0.0); float shadow=pow(saturate(1.0-l),max(Radius,0.01))*ShadowAmount; float highlight=pow(saturate(l),max(Radius,0.01))*HighlightAmount; float3 v=s.rgb*(1.0+shadow)/(1.0+highlight*max(l,0.0)); return float4(lerp(s.rgb,v,saturate(Strength)),s.a);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["ShadowAmount",1],["HighlightAmount",1],["Radius",1],["Strength",1]],"outs":[["Resultat",4]]},{"id":9,"op":"output","pos":[780,40],"in":[[8,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo 0
//@param ShadowAmount float 0.25 0 2 "Shadow Amount"
//@param HighlightAmount float 0.25 0 2 "Highlight Amount"
//@param Radius float 2 0.01 8 "Radius"
//@param Strength float 1 0 1 "Strength"

// Imported preset (Effect / Color & Tone): Recovers shadows and compresses highlights while retaining HDR detail.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float ShadowAmount, float HighlightAmount, float Radius, float Strength)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float l=max(dot(s.rgb,float3(.2126,.7152,.0722)),0.0); float shadow=pow(saturate(1.0-l),max(Radius,0.01))*ShadowAmount; float highlight=pow(saturate(l),max(Radius,0.01))*HighlightAmount; float3 v=s.rgb*(1.0+shadow)/(1.0+highlight*max(l,0.0)); return float4(lerp(s.rgb,v,saturate(Strength)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), ShadowAmount, HighlightAmount, Radius, Strength);
}
