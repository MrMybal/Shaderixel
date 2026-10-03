//@name Sharpen
//@category Traitements
//@graph {"name":"Sharpen","category":"Traitements","inputs":1,"premultiplied":false,"nextId":7,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.5,0.0,0.0,0.0],"name":"Strength","text":"Strength","index":1},{"id":5,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0]],"value":[0,0,0,0],"name":"Sharpen","text":"    float2 t=float2(1.0/max(CyCanvasSize.x,1.0),1.0/max(CyCanvasSize.y,1.0)); float4 c=Texture2DSample(CyInput,CyInputSampler,UV); float4 n=Texture2DSample(CyInput,CyInputSampler,UV+float2(t.x,0))+Texture2DSample(CyInput,CyInputSampler,UV-float2(t.x,0))+Texture2DSample(CyInput,CyInputSampler,UV+float2(0,t.y))+Texture2DSample(CyInput,CyInputSampler,UV-float2(0,t.y)); return lerp(c,c*5.0-n,saturate(Strength));","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Strength",1]],"outs":[["Resultat",4]]},{"id":6,"op":"output","pos":[780,40],"in":[[5,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo full
//@param Strength float 0.5 0 1 "Strength"

// Imported preset (Effect / Detail): Five-tap sharpen filter with live strength.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Strength)
{
    float2 t=float2(1.0/max(CyCanvasSize.x,1.0),1.0/max(CyCanvasSize.y,1.0)); float4 c=Texture2DSample(CyInput,CyInputSampler,UV); float4 n=Texture2DSample(CyInput,CyInputSampler,UV+float2(t.x,0))+Texture2DSample(CyInput,CyInputSampler,UV-float2(t.x,0))+Texture2DSample(CyInput,CyInputSampler,UV+float2(0,t.y))+Texture2DSample(CyInput,CyInputSampler,UV-float2(0,t.y)); return lerp(c,c*5.0-n,saturate(Strength));
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Strength);
}
