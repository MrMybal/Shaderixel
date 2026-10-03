//@name Color Balance
//@category Traitements
//@graph {"name":"Color Balance","category":"Traitements","inputs":1,"premultiplied":false,"nextId":11,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"PreserveLuminosity","text":"Preserve Luminosity","index":1},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Strength","text":"Strength","index":1},{"id":6,"op":"paramcolor","pos":[40,550],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"ShadowBalance","text":"Shadow Balance","index":1},{"id":7,"op":"paramcolor","pos":[40,660],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"MidtoneBalance","text":"Midtone Balance","index":1},{"id":8,"op":"paramcolor","pos":[40,770],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"HighlightBalance","text":"Highlight Balance","index":1},{"id":9,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0]],"value":[0,0,0,0],"name":"Color Balance","text":"    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float l=dot(s.rgb,float3(.2126,.7152,.0722)); float sh=saturate((0.5-l)*2.0); float hi=saturate((l-0.5)*2.0); float mid=saturate(1.0-sh-hi); float3 v=s.rgb+ShadowBalance.rgb*sh+MidtoneBalance.rgb*mid+HighlightBalance.rgb*hi; float vl=dot(v,float3(.2126,.7152,.0722)); v+=PreserveLuminosity*(l-vl); return float4(lerp(s.rgb,v,saturate(Strength)),s.a);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["PreserveLuminosity",1],["Strength",1],["ShadowBalance",3],["MidtoneBalance",3],["HighlightBalance",3]],"outs":[["Resultat",4]]},{"id":10,"op":"output","pos":[780,40],"in":[[9,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo 0
//@param PreserveLuminosity float 1 0 1 "Preserve Luminosity"
//@param Strength float 1 0 1 "Strength"
//@param ShadowBalance color 0 0 0 0 "Shadow Balance"
//@param MidtoneBalance color 0 0 0 0 "Midtone Balance"
//@param HighlightBalance color 0 0 0 0 "Highlight Balance"

// Imported preset (Effect / Color & Tone): Independent shadow, midtone, and highlight color balance with luminosity preservation.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float PreserveLuminosity, float Strength, float3 ShadowBalance, float3 MidtoneBalance, float3 HighlightBalance)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float l=dot(s.rgb,float3(.2126,.7152,.0722)); float sh=saturate((0.5-l)*2.0); float hi=saturate((l-0.5)*2.0); float mid=saturate(1.0-sh-hi); float3 v=s.rgb+ShadowBalance.rgb*sh+MidtoneBalance.rgb*mid+HighlightBalance.rgb*hi; float vl=dot(v,float3(.2126,.7152,.0722)); v+=PreserveLuminosity*(l-vl); return float4(lerp(s.rgb,v,saturate(Strength)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), PreserveLuminosity, Strength, ShadowBalance.rgb, MidtoneBalance.rgb, HighlightBalance.rgb);
}
