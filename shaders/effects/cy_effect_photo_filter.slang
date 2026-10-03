//@name Photo Filter
//@category Traitements
//@graph {"name":"Photo Filter","category":"Traitements","inputs":1,"premultiplied":false,"nextId":9,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.25,0.0,0.0,0.0],"name":"Density","text":"Density","index":1},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"PreserveLuminosity","text":"Preserve Luminosity","index":1},{"id":6,"op":"paramcolor","pos":[40,550],"in":[],"value":[1.0,0.550000012,0.200000003,1.0],"name":"FilterColor","text":"Filter Color","index":1},{"id":7,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0]],"value":[0,0,0,0],"name":"Photo Filter","text":"    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float l=dot(s.rgb,float3(.2126,.7152,.0722)); float3 v=lerp(s.rgb,s.rgb*FilterColor.rgb,saturate(Density)); float vl=dot(v,float3(.2126,.7152,.0722)); v+=PreserveLuminosity*(l-vl); return float4(v,s.a);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Density",1],["PreserveLuminosity",1],["FilterColor",3]],"outs":[["Resultat",4]]},{"id":8,"op":"output","pos":[780,40],"in":[[7,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo 0
//@param Density float 0.25 0 1 "Density"
//@param PreserveLuminosity float 1 0 1 "Preserve Luminosity"
//@param FilterColor color 1 0.55 0.2 1 "Filter Color"

// Imported preset (Effect / Color & Tone): Photographic warming/cooling filter with density and optional luminance preservation.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Density, float PreserveLuminosity, float3 FilterColor)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float l=dot(s.rgb,float3(.2126,.7152,.0722)); float3 v=lerp(s.rgb,s.rgb*FilterColor.rgb,saturate(Density)); float vl=dot(v,float3(.2126,.7152,.0722)); v+=PreserveLuminosity*(l-vl); return float4(v,s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Density, PreserveLuminosity, FilterColor.rgb);
}
