//@name Edge Detect
//@category Traitements
//@graph {"name":"Edge Detect","category":"Traitements","inputs":1,"premultiplied":false,"nextId":10,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Radius","text":"Radius","index":1,"min":0.25,"max":16.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[2.0,0.0,0.0,0.0],"name":"Strength","text":"Strength","index":1,"min":0.0,"max":32.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Blend","text":"Blend","index":1},{"id":7,"op":"paramcolor","pos":[40,660],"in":[],"value":[1.0,1.0,1.0,1.0],"name":"EdgeColor","text":"Edge Color","index":1},{"id":8,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0]],"value":[0,0,0,0],"name":"Edge Detect","text":"    float2 t=float2(1.0/max(CyCanvasSize.x,1.0),1.0/max(CyCanvasSize.y,1.0))*Radius; float tl=dot(Texture2DSample(CyInput,CyInputSampler,UV+float2(-t.x,-t.y)).rgb,float3(.2126,.7152,.0722)); float tc=dot(Texture2DSample(CyInput,CyInputSampler,UV+float2(0,-t.y)).rgb,float3(.2126,.7152,.0722)); float tr=dot(Texture2DSample(CyInput,CyInputSampler,UV+float2(t.x,-t.y)).rgb,float3(.2126,.7152,.0722)); float ml=dot(Texture2DSample(CyInput,CyInputSampler,UV+float2(-t.x,0)).rgb,float3(.2126,.7152,.0722)); float mr=dot(Texture2DSample(CyInput,CyInputSampler,UV+float2(t.x,0)).rgb,float3(.2126,.7152,.0722)); float bl=dot(Texture2DSample(CyInput,CyInputSampler,UV+float2(-t.x,t.y)).rgb,float3(.2126,.7152,.0722)); float bc=dot(Texture2DSample(CyInput,CyInputSampler,UV+float2(0,t.y)).rgb,float3(.2126,.7152,.0722)); float br=dot(Texture2DSample(CyInput,CyInputSampler,UV+t).rgb,float3(.2126,.7152,.0722)); float gx=-tl-2*ml-bl+tr+2*mr+br; float gy=-tl-2*tc-tr+bl+2*bc+br; float e=saturate(length(float2(gx,gy))*Strength); float4 src=Texture2DSample(CyInput,CyInputSampler,UV); return float4(lerp(src.rgb,EdgeColor.rgb*e,saturate(Blend)),src.a);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Radius",1],["Strength",1],["Blend",1],["EdgeColor",3]],"outs":[["Resultat",4]]},{"id":9,"op":"output","pos":[780,40],"in":[[8,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo full
//@param Radius float 1 0.25 16 "Radius"
//@param Strength float 2 0 32 "Strength"
//@param Blend float 1 0 1 "Blend"
//@param EdgeColor color 1 1 1 1 "Edge Color"

// Imported preset (Effect / Detail): Sobel edge detection with source overlay.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float Strength, float Blend, float3 EdgeColor)
{
    float2 t=float2(1.0/max(CyCanvasSize.x,1.0),1.0/max(CyCanvasSize.y,1.0))*Radius; float tl=dot(Texture2DSample(CyInput,CyInputSampler,UV+float2(-t.x,-t.y)).rgb,float3(.2126,.7152,.0722)); float tc=dot(Texture2DSample(CyInput,CyInputSampler,UV+float2(0,-t.y)).rgb,float3(.2126,.7152,.0722)); float tr=dot(Texture2DSample(CyInput,CyInputSampler,UV+float2(t.x,-t.y)).rgb,float3(.2126,.7152,.0722)); float ml=dot(Texture2DSample(CyInput,CyInputSampler,UV+float2(-t.x,0)).rgb,float3(.2126,.7152,.0722)); float mr=dot(Texture2DSample(CyInput,CyInputSampler,UV+float2(t.x,0)).rgb,float3(.2126,.7152,.0722)); float bl=dot(Texture2DSample(CyInput,CyInputSampler,UV+float2(-t.x,t.y)).rgb,float3(.2126,.7152,.0722)); float bc=dot(Texture2DSample(CyInput,CyInputSampler,UV+float2(0,t.y)).rgb,float3(.2126,.7152,.0722)); float br=dot(Texture2DSample(CyInput,CyInputSampler,UV+t).rgb,float3(.2126,.7152,.0722)); float gx=-tl-2*ml-bl+tr+2*mr+br; float gy=-tl-2*tc-tr+bl+2*bc+br; float e=saturate(length(float2(gx,gy))*Strength); float4 src=Texture2DSample(CyInput,CyInputSampler,UV); return float4(lerp(src.rgb,EdgeColor.rgb*e,saturate(Blend)),src.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, Strength, Blend, EdgeColor.rgb);
}
