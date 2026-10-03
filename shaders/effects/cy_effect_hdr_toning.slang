//@name HDR Toning
//@category Traitements
//@graph {"name":"HDR Toning","category":"Traitements","inputs":1,"premultiplied":false,"nextId":12,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Exposure","text":"Exposure","index":1,"min":-16.0,"max":16.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Detail","text":"Detail","index":1,"min":0.0,"max":8.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.5,0.0,0.0,0.0],"name":"Pivot","text":"Pivot","index":1,"min":0.0,"max":4.0},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Shoulder","text":"Shoulder","index":1,"min":0.00100000005,"max":16.0},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Saturation","text":"Saturation","index":1,"min":0.0,"max":4.0},{"id":9,"op":"param","pos":[40,880],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Strength","text":"Strength","index":1},{"id":10,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0]],"value":[0,0,0,0],"name":"HDR Toning","text":"    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float3 x=max(s.rgb*exp2(Exposure),0.0); float l=dot(x,float3(.2126,.7152,.0722)); x=(x-Pivot)*Detail+Pivot; x=x/(1.0+x/max(Shoulder,0.001)); float nl=dot(x,float3(.2126,.7152,.0722)); x=lerp(nl.xxx,x,Saturation); return float4(lerp(s.rgb,x,saturate(Strength)),s.a);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Exposure",1],["Detail",1],["Pivot",1],["Shoulder",1],["Saturation",1],["Strength",1]],"outs":[["Resultat",4]]},{"id":11,"op":"output","pos":[780,40],"in":[[10,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo 0
//@param Exposure float 0 -16 16 "Exposure"
//@param Detail float 1 0 8 "Detail"
//@param Pivot float 0.5 0 4 "Pivot"
//@param Shoulder float 1 0.001 16 "Shoulder"
//@param Saturation float 1 0 4 "Saturation"
//@param Strength float 1 0 1 "Strength"

// Imported preset (Effect / Color & Tone): HDR exposure, detail contrast, shoulder compression, and saturation toning.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Exposure, float Detail, float Pivot, float Shoulder, float Saturation, float Strength)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV); float3 x=max(s.rgb*exp2(Exposure),0.0); float l=dot(x,float3(.2126,.7152,.0722)); x=(x-Pivot)*Detail+Pivot; x=x/(1.0+x/max(Shoulder,0.001)); float nl=dot(x,float3(.2126,.7152,.0722)); x=lerp(nl.xxx,x,Saturation); return float4(lerp(s.rgb,x,saturate(Strength)),s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Exposure, Detail, Pivot, Shoulder, Saturation, Strength);
}
