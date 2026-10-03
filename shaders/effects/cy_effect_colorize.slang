//@name Colorize
//@category Traitements
//@graph {"name":"Colorize","category":"Traitements","inputs":1,"premultiplied":false,"nextId":10,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Gamma","text":"Gamma","index":1,"min":0.00999999978,"max":8.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Strength","text":"Strength","index":1},{"id":6,"op":"paramcolor","pos":[40,550],"in":[],"value":[0.0199999996,0.0399999991,0.119999997,1.0],"name":"ShadowColor","text":"Shadow Color","index":1},{"id":7,"op":"paramcolor","pos":[40,660],"in":[],"value":[1.0,0.349999994,0.0799999982,1.0],"name":"HighlightColor","text":"Highlight Color","index":1},{"id":8,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0]],"value":[0,0,0,0],"name":"Colorize","text":"    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float l=saturate(dot(s.rgb,float3(.2126,.7152,.0722))); float3 v=lerp(ShadowColor.rgb,HighlightColor.rgb,pow(l,max(Gamma,0.01))); return float4(lerp(s.rgb,v,saturate(Strength)),s.a);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Gamma",1],["Strength",1],["ShadowColor",3],["HighlightColor",3]],"outs":[["Resultat",4]]},{"id":9,"op":"output","pos":[780,40],"in":[[8,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo 0
//@param Gamma float 1 0.01 8 "Gamma"
//@param Strength float 1 0 1 "Strength"
//@param ShadowColor color 0.02 0.04 0.12 1 "Shadow Color"
//@param HighlightColor color 1 0.35 0.08 1 "Highlight Color"

// Imported preset (Effect / Color & Tone): Maps luminance between two HDR colors.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Gamma, float Strength, float3 ShadowColor, float3 HighlightColor)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float l=saturate(dot(s.rgb,float3(.2126,.7152,.0722))); float3 v=lerp(ShadowColor.rgb,HighlightColor.rgb,pow(l,max(Gamma,0.01))); return float4(lerp(s.rgb,v,saturate(Strength)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Gamma, Strength, ShadowColor.rgb, HighlightColor.rgb);
}
