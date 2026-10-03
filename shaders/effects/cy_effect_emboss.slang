//@name Emboss
//@category Traitements
//@graph {"name":"Emboss","category":"Traitements","inputs":1,"premultiplied":false,"nextId":11,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.785000026,0.0,0.0,0.0],"name":"Angle","text":"Angle","index":1,"min":-6.28299999,"max":6.28299999},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Radius","text":"Radius","index":1,"min":0.25,"max":32.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[2.0,0.0,0.0,0.0],"name":"Strength","text":"Strength","index":1,"min":0.0,"max":16.0},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[0.5,0.0,0.0,0.0],"name":"Bias","text":"Bias","index":1,"min":-2.0,"max":2.0},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Blend","text":"Blend","index":1},{"id":9,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0]],"value":[0,0,0,0],"name":"Emboss","text":"    float2 t=float2(cos(Angle)/max(CyCanvasSize.x,1.0),sin(Angle)/max(CyCanvasSize.y,1.0))*Radius; float4 a=Texture2DSample(CyInput,CyInputSampler,UV-t); float4 b=Texture2DSample(CyInput,CyInputSampler,UV+t); float3 e=(a.rgb-b.rgb)*Strength+Bias; float4 s=Texture2DSample(CyInput,CyInputSampler,UV); return float4(lerp(s.rgb,e,saturate(Blend)),s.a);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Angle",1],["Radius",1],["Strength",1],["Bias",1],["Blend",1]],"outs":[["Resultat",4]]},{"id":10,"op":"output","pos":[780,40],"in":[[9,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo full
//@param Angle float 0.785 -6.283 6.283 "Angle"
//@param Radius float 1 0.25 32 "Radius"
//@param Strength float 2 0 16 "Strength"
//@param Bias float 0.5 -2 2 "Bias"
//@param Blend float 1 0 1 "Blend"

// Imported preset (Effect / Detail): Directional emboss filter with adjustable bias.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Angle, float Radius, float Strength, float Bias, float Blend)
{
    float2 t=float2(cos(Angle)/max(CyCanvasSize.x,1.0),sin(Angle)/max(CyCanvasSize.y,1.0))*Radius; float4 a=Texture2DSample(CyInput,CyInputSampler,UV-t); float4 b=Texture2DSample(CyInput,CyInputSampler,UV+t); float3 e=(a.rgb-b.rgb)*Strength+Bias; float4 s=Texture2DSample(CyInput,CyInputSampler,UV); return float4(lerp(s.rgb,e,saturate(Blend)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Angle, Radius, Strength, Bias, Blend);
}
