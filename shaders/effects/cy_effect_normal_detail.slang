//@name Normal Detail
//@category Traitements
//@graph {"name":"Normal Detail","category":"Traitements","inputs":1,"premultiplied":false,"nextId":9,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Radius","text":"Radius","index":1,"min":0.25,"max":16.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[3.0,0.0,0.0,0.0],"name":"DetailStrength","text":"Detail Strength","index":1,"min":0.0,"max":64.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"UseAlpha","text":"Use Alpha","index":1},{"id":7,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0]],"value":[0,0,0,0],"name":"Normal Detail","text":"    float2 t=Radius/max(CyCanvasSize.xy,1.0);float4 s=Texture2DSample(CyInput,CyInputSampler,UV);float hl=lerp(dot(Texture2DSample(CyInput,CyInputSampler,UV-float2(t.x,0)).rgb,float3(.2126,.7152,.0722)),Texture2DSample(CyInput,CyInputSampler,UV-float2(t.x,0)).a,UseAlpha);float hr=lerp(dot(Texture2DSample(CyInput,CyInputSampler,UV+float2(t.x,0)).rgb,float3(.2126,.7152,.0722)),Texture2DSample(CyInput,CyInputSampler,UV+float2(t.x,0)).a,UseAlpha);float hu=lerp(dot(Texture2DSample(CyInput,CyInputSampler,UV-float2(0,t.y)).rgb,float3(.2126,.7152,.0722)),Texture2DSample(CyInput,CyInputSampler,UV-float2(0,t.y)).a,UseAlpha);float hd=lerp(dot(Texture2DSample(CyInput,CyInputSampler,UV+float2(0,t.y)).rgb,float3(.2126,.7152,.0722)),Texture2DSample(CyInput,CyInputSampler,UV+float2(0,t.y)).a,UseAlpha);float3 base=normalize(s.rgb*2.0-1.0);float3 detail=normalize(float3((hl-hr)*DetailStrength,(hu-hd)*DetailStrength,1.0));float3 combined=normalize(float3(base.xy+detail.xy,base.z*detail.z));return float4(combined*.5+.5,s.a);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Radius",1],["DetailStrength",1],["UseAlpha",1]],"outs":[["Resultat",4]]},{"id":8,"op":"output","pos":[780,40],"in":[[7,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo full
//@param Radius float 1 0.25 16 "Radius"
//@param DetailStrength float 3 0 64 "Detail Strength"
//@param UseAlpha float 1 0 1 "Use Alpha"

// Imported preset (Effect / PBR): Adds high-frequency detail stored in alpha or luminance to an existing tangent-space normal map.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float DetailStrength, float UseAlpha)
{
    float2 t=Radius/max(CyCanvasSize.xy,1.0);float4 s=Texture2DSample(CyInput,CyInputSampler,UV);float hl=lerp(dot(Texture2DSample(CyInput,CyInputSampler,UV-float2(t.x,0)).rgb,float3(.2126,.7152,.0722)),Texture2DSample(CyInput,CyInputSampler,UV-float2(t.x,0)).a,UseAlpha);float hr=lerp(dot(Texture2DSample(CyInput,CyInputSampler,UV+float2(t.x,0)).rgb,float3(.2126,.7152,.0722)),Texture2DSample(CyInput,CyInputSampler,UV+float2(t.x,0)).a,UseAlpha);float hu=lerp(dot(Texture2DSample(CyInput,CyInputSampler,UV-float2(0,t.y)).rgb,float3(.2126,.7152,.0722)),Texture2DSample(CyInput,CyInputSampler,UV-float2(0,t.y)).a,UseAlpha);float hd=lerp(dot(Texture2DSample(CyInput,CyInputSampler,UV+float2(0,t.y)).rgb,float3(.2126,.7152,.0722)),Texture2DSample(CyInput,CyInputSampler,UV+float2(0,t.y)).a,UseAlpha);float3 base=normalize(s.rgb*2.0-1.0);float3 detail=normalize(float3((hl-hr)*DetailStrength,(hu-hd)*DetailStrength,1.0));float3 combined=normalize(float3(base.xy+detail.xy,base.z*detail.z));return float4(combined*.5+.5,s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, DetailStrength, UseAlpha);
}
