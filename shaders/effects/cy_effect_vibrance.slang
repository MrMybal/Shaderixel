//@name Vibrance
//@category Traitements
//@graph {"name":"Vibrance","category":"Traitements","inputs":1,"premultiplied":false,"nextId":8,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.5,0.0,0.0,0.0],"name":"Vibrance","text":"Vibrance","index":1,"min":-2.0,"max":2.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Strength","text":"Strength","index":1},{"id":6,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0]],"value":[0,0,0,0],"name":"Vibrance","text":"    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float mx=max(max(s.r,s.g),s.b); float mn=min(min(s.r,s.g),s.b); float sat=mx-mn; float l=dot(s.rgb,float3(.2126,.7152,.0722)); float amount=Vibrance*(1.0-saturate(sat)); float3 v=lerp(l.xxx,s.rgb,1.0+amount); return float4(lerp(s.rgb,v,saturate(Strength)),s.a);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Vibrance",1],["Strength",1]],"outs":[["Resultat",4]]},{"id":7,"op":"output","pos":[780,40],"in":[[6,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo 0
//@param Vibrance float 0.5 -2 2 "Vibrance"
//@param Strength float 1 0 1 "Strength"

// Imported preset (Effect / Color & Tone): Adaptive saturation that protects already saturated colors.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Vibrance, float Strength)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float mx=max(max(s.r,s.g),s.b); float mn=min(min(s.r,s.g),s.b); float sat=mx-mn; float l=dot(s.rgb,float3(.2126,.7152,.0722)); float amount=Vibrance*(1.0-saturate(sat)); float3 v=lerp(l.xxx,s.rgb,1.0+amount); return float4(lerp(s.rgb,v,saturate(Strength)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Vibrance, Strength);
}
