//@name Channel Mixer
//@category Traitements
//@graph {"name":"Channel Mixer","category":"Traitements","inputs":1,"premultiplied":false,"nextId":11,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Strength","text":"Strength","index":1},{"id":5,"op":"paramcolor","pos":[40,440],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"RedMix","text":"Red Mix","index":1},{"id":6,"op":"paramcolor","pos":[40,550],"in":[],"value":[0.0,1.0,0.0,0.0],"name":"GreenMix","text":"Green Mix","index":1},{"id":7,"op":"paramcolor","pos":[40,660],"in":[],"value":[0.0,0.0,1.0,0.0],"name":"BlueMix","text":"Blue Mix","index":1},{"id":8,"op":"paramcolor","pos":[40,770],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Constant","text":"Constant","index":1},{"id":9,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0]],"value":[0,0,0,0],"name":"Channel Mixer","text":"    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float3 v=float3(dot(s.rgb,RedMix.rgb),dot(s.rgb,GreenMix.rgb),dot(s.rgb,BlueMix.rgb))+Constant.rgb; return float4(lerp(s.rgb,v,saturate(Strength)),s.a);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Strength",1],["RedMix",3],["GreenMix",3],["BlueMix",3],["Constant",3]],"outs":[["Resultat",4]]},{"id":10,"op":"output","pos":[780,40],"in":[[9,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo 0
//@param Strength float 1 0 1 "Strength"
//@param RedMix color 1 0 0 0 "Red Mix"
//@param GreenMix color 0 1 0 0 "Green Mix"
//@param BlueMix color 0 0 1 0 "Blue Mix"
//@param Constant color 0 0 0 0 "Constant"

// Imported preset (Effect / Color & Tone): Full RGB channel mixer using three editable matrix rows.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Strength, float3 RedMix, float3 GreenMix, float3 BlueMix, float3 Constant)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float3 v=float3(dot(s.rgb,RedMix.rgb),dot(s.rgb,GreenMix.rgb),dot(s.rgb,BlueMix.rgb))+Constant.rgb; return float4(lerp(s.rgb,v,saturate(Strength)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Strength, RedMix.rgb, GreenMix.rgb, BlueMix.rgb, Constant.rgb);
}
