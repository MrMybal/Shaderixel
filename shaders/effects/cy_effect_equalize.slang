//@name Equalize
//@category Traitements
//@graph {"name":"Equalize","category":"Traitements","inputs":1,"premultiplied":false,"nextId":9,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.5,0.0,0.0,0.0],"name":"Midpoint","text":"Midpoint","index":1,"min":0.0,"max":2.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[6.0,0.0,0.0,0.0],"name":"Contrast","text":"Contrast","index":1,"min":0.00999999978,"max":32.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Strength","text":"Strength","index":1},{"id":7,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0]],"value":[0,0,0,0],"name":"Equalize","text":"    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float l=dot(s.rgb,float3(.2126,.7152,.0722)); float mapped=1.0/(1.0+exp(-(l-Midpoint)*Contrast)); float reference=1.0/(1.0+exp(Midpoint*Contrast)); mapped=saturate((mapped-reference)/max(1.0-reference,0.0001)); float3 v=s.rgb*(mapped/max(l,0.0001)); return float4(lerp(s.rgb,v,saturate(Strength)),s.a);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Midpoint",1],["Contrast",1],["Strength",1]],"outs":[["Resultat",4]]},{"id":8,"op":"output","pos":[780,40],"in":[[7,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo 0
//@param Midpoint float 0.5 0 2 "Midpoint"
//@param Contrast float 6 0.01 32 "Contrast"
//@param Strength float 1 0 1 "Strength"

// Imported preset (Effect / Color & Tone): Adaptive sigmoidal contrast equalization suitable for SDR and HDR sources.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Midpoint, float Contrast, float Strength)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float l=dot(s.rgb,float3(.2126,.7152,.0722)); float mapped=1.0/(1.0+exp(-(l-Midpoint)*Contrast)); float reference=1.0/(1.0+exp(Midpoint*Contrast)); mapped=saturate((mapped-reference)/max(1.0-reference,0.0001)); float3 v=s.rgb*(mapped/max(l,0.0001)); return float4(lerp(s.rgb,v,saturate(Strength)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Midpoint, Contrast, Strength);
}
