//@name Curvature From Normal
//@category Traitements
//@graph {"name":"Curvature From Normal","category":"Traitements","inputs":1,"premultiplied":false,"nextId":11,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Radius","text":"Radius","index":1,"min":0.25,"max":16.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[3.0,0.0,0.0,0.0],"name":"Strength","text":"Strength","index":1,"min":0.0,"max":64.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Bias","text":"Bias","index":1,"min":-1.0,"max":1.0},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Concave","text":"Concave","index":1},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Power","text":"Power","index":1,"min":0.0500000007,"max":8.0},{"id":9,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0]],"value":[0,0,0,0],"name":"Curvature From Normal","text":"    float2 t=Radius/max(CyCanvasSize.xy,1.0);float3 c=normalize(Texture2DSample(CyInput,CyInputSampler,UV).xyz*2.0-1.0);float3 l=normalize(Texture2DSample(CyInput,CyInputSampler,UV-float2(t.x,0)).xyz*2.0-1.0);float3 r=normalize(Texture2DSample(CyInput,CyInputSampler,UV+float2(t.x,0)).xyz*2.0-1.0);float3 u=normalize(Texture2DSample(CyInput,CyInputSampler,UV-float2(0,t.y)).xyz*2.0-1.0);float3 d=normalize(Texture2DSample(CyInput,CyInputSampler,UV+float2(0,t.y)).xyz*2.0-1.0);float curvature=((r.x-l.x)+(d.y-u.y))*Strength;float convex=saturate(curvature+Bias),concave=saturate(-curvature-Bias);float value=lerp(convex,concave,Concave);value=pow(value,Power);return float4(value.xxx,1);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Radius",1],["Strength",1],["Bias",1],["Concave",1],["Power",1]],"outs":[["Resultat",4]]},{"id":10,"op":"output","pos":[780,40],"in":[[9,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo full
//@param Radius float 1 0.25 16 "Radius"
//@param Strength float 3 0 64 "Strength"
//@param Bias float 0 -1 1 "Bias"
//@param Concave float 0 0 1 "Concave"
//@param Power float 1 0.05 8 "Power"

// Imported preset (Effect / PBR): Extracts convex and concave curvature masks from a tangent-space normal map.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float Strength, float Bias, float Concave, float Power)
{
    float2 t=Radius/max(CyCanvasSize.xy,1.0);float3 c=normalize(Texture2DSample(CyInput,CyInputSampler,UV).xyz*2.0-1.0);float3 l=normalize(Texture2DSample(CyInput,CyInputSampler,UV-float2(t.x,0)).xyz*2.0-1.0);float3 r=normalize(Texture2DSample(CyInput,CyInputSampler,UV+float2(t.x,0)).xyz*2.0-1.0);float3 u=normalize(Texture2DSample(CyInput,CyInputSampler,UV-float2(0,t.y)).xyz*2.0-1.0);float3 d=normalize(Texture2DSample(CyInput,CyInputSampler,UV+float2(0,t.y)).xyz*2.0-1.0);float curvature=((r.x-l.x)+(d.y-u.y))*Strength;float convex=saturate(curvature+Bias),concave=saturate(-curvature-Bias);float value=lerp(convex,concave,Concave);value=pow(value,Power);return float4(value.xxx,1);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, Strength, Bias, Concave, Power);
}
