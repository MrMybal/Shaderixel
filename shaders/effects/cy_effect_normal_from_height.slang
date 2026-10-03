//@name Normal From Height
//@category Traitements
//@graph {"name":"Normal From Height","category":"Traitements","inputs":1,"premultiplied":false,"nextId":8,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Radius","text":"Radius","index":1,"min":0.25,"max":16.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[4.0,0.0,0.0,0.0],"name":"Strength","text":"Strength","index":1,"min":0.00999999978,"max":64.0},{"id":6,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0]],"value":[0,0,0,0],"name":"Normal From Height","text":"    float2 t=float2(1.0/max(CyCanvasSize.x,1.0),1.0/max(CyCanvasSize.y,1.0))*Radius; float l=dot(Texture2DSample(CyInput,CyInputSampler,UV-float2(t.x,0)).rgb,float3(.2126,.7152,.0722)); float r=dot(Texture2DSample(CyInput,CyInputSampler,UV+float2(t.x,0)).rgb,float3(.2126,.7152,.0722)); float u=dot(Texture2DSample(CyInput,CyInputSampler,UV-float2(0,t.y)).rgb,float3(.2126,.7152,.0722)); float d=dot(Texture2DSample(CyInput,CyInputSampler,UV+float2(0,t.y)).rgb,float3(.2126,.7152,.0722)); float3 n=normalize(float3((l-r)*Strength,(u-d)*Strength,1.0)); float a=Texture2DSample(CyInput,CyInputSampler,UV).a; return float4(n*0.5+0.5,a);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Radius",1],["Strength",1]],"outs":[["Resultat",4]]},{"id":7,"op":"output","pos":[780,40],"in":[[6,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo full
//@param Radius float 1 0.25 16 "Radius"
//@param Strength float 4 0.01 64 "Strength"

// Imported preset (Effect / Detail): Generates a tangent-space normal map from source luminance.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float Strength)
{
    float2 t=float2(1.0/max(CyCanvasSize.x,1.0),1.0/max(CyCanvasSize.y,1.0))*Radius; float l=dot(Texture2DSample(CyInput,CyInputSampler,UV-float2(t.x,0)).rgb,float3(.2126,.7152,.0722)); float r=dot(Texture2DSample(CyInput,CyInputSampler,UV+float2(t.x,0)).rgb,float3(.2126,.7152,.0722)); float u=dot(Texture2DSample(CyInput,CyInputSampler,UV-float2(0,t.y)).rgb,float3(.2126,.7152,.0722)); float d=dot(Texture2DSample(CyInput,CyInputSampler,UV+float2(0,t.y)).rgb,float3(.2126,.7152,.0722)); float3 n=normalize(float3((l-r)*Strength,(u-d)*Strength,1.0)); float a=Texture2DSample(CyInput,CyInputSampler,UV).a; return float4(n*0.5+0.5,a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, Strength);
}
