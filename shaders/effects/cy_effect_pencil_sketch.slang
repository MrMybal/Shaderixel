//@name Pencil Sketch
//@category Traitements
//@graph {"name":"Pencil Sketch","category":"Traitements","inputs":1,"premultiplied":false,"nextId":16,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[1.5,0.0,0.0,0.0],"name":"Radius","text":"Radius","index":1,"min":0.25,"max":16.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[5.0,0.0,0.0,0.0],"name":"EdgeStrength","text":"Edge Strength","index":1,"min":0.0,"max":64.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.699999988,0.0,0.0,0.0],"name":"EdgePower","text":"Edge Power","index":1,"min":0.0500000007,"max":8.0},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[0.449999988,0.0,0.0,0.0],"name":"Shading","text":"Shading","index":1,"min":0.0,"max":2.0},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[512.0,0.0,0.0,0.0],"name":"GrainScale","text":"Grain Scale","index":1,"min":2.0,"max":4096.0},{"id":9,"op":"param","pos":[40,880],"in":[],"value":[0.0799999982,0.0,0.0,0.0],"name":"GrainAmount","text":"Grain Amount","index":1},{"id":10,"op":"param","pos":[40,990],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"PreserveColor","text":"Preserve Color","index":1},{"id":11,"op":"param","pos":[40,1100],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Strength","text":"Strength","index":1},{"id":12,"op":"paramcolor","pos":[40,1210],"in":[],"value":[0.0149999997,0.0120000001,0.00999999978,1.0],"name":"PencilColor","text":"Pencil Color","index":1},{"id":13,"op":"paramcolor","pos":[40,1320],"in":[],"value":[0.899999976,0.860000014,0.74000001,1.0],"name":"PaperColor","text":"Paper Color","index":1},{"id":14,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0],[11,0],[12,0],[13,0]],"value":[0,0,0,0],"name":"Pencil Sketch","text":"    float2 t=Radius/max(CyCanvasSize.xy,1.0);float4 c=Texture2DSample(CyInput,CyInputSampler,UV);float l=dot(c.rgb,float3(.2126,.7152,.0722));float lx=dot(Texture2DSample(CyInput,CyInputSampler,UV+float2(t.x,0)).rgb,float3(.2126,.7152,.0722))-dot(Texture2DSample(CyInput,CyInputSampler,UV-float2(t.x,0)).rgb,float3(.2126,.7152,.0722));float ly=dot(Texture2DSample(CyInput,CyInputSampler,UV+float2(0,t.y)).rgb,float3(.2126,.7152,.0722))-dot(Texture2DSample(CyInput,CyInputSampler,UV-float2(0,t.y)).rgb,float3(.2126,.7152,.0722));float edge=pow(saturate(length(float2(lx,ly))*EdgeStrength),EdgePower);float grain=frac(sin(dot(floor(UV*GrainScale),float2(12.9898,78.233)))*43758.5453);float shade=saturate(1.0-edge-(1.0-l)*Shading-(grain-.5)*GrainAmount);float3 mono=lerp(PencilColor.rgb,PaperColor.rgb,shade);float3 outColor=lerp(mono,mono*c.rgb*1.5,PreserveColor);return float4(lerp(c.rgb,outColor,Strength),c.a);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Radius",1],["EdgeStrength",1],["EdgePower",1],["Shading",1],["GrainScale",1],["GrainAmount",1],["PreserveColor",1],["Strength",1],["PencilColor",3],["PaperColor",3]],"outs":[["Resultat",4]]},{"id":15,"op":"output","pos":[780,40],"in":[[14,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo full
//@param Radius float 1.5 0.25 16 "Radius"
//@param EdgeStrength float 5 0 64 "Edge Strength"
//@param EdgePower float 0.7 0.05 8 "Edge Power"
//@param Shading float 0.45 0 2 "Shading"
//@param GrainScale float 512 2 4096 "Grain Scale"
//@param GrainAmount float 0.08 0 1 "Grain Amount"
//@param PreserveColor float 0 0 1 "Preserve Color"
//@param Strength float 1 0 1 "Strength"
//@param PencilColor color 0.015 0.012 0.01 1 "Pencil Color"
//@param PaperColor color 0.9 0.86 0.74 1 "Paper Color"

// Imported preset (Effect / Stylize): Pencil-sketch conversion with multi-scale edges, paper tone, grain, and source-color retention.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float EdgeStrength, float EdgePower, float Shading, float GrainScale, float GrainAmount, float PreserveColor, float Strength, float3 PencilColor, float3 PaperColor)
{
    float2 t=Radius/max(CyCanvasSize.xy,1.0);float4 c=Texture2DSample(CyInput,CyInputSampler,UV);float l=dot(c.rgb,float3(.2126,.7152,.0722));float lx=dot(Texture2DSample(CyInput,CyInputSampler,UV+float2(t.x,0)).rgb,float3(.2126,.7152,.0722))-dot(Texture2DSample(CyInput,CyInputSampler,UV-float2(t.x,0)).rgb,float3(.2126,.7152,.0722));float ly=dot(Texture2DSample(CyInput,CyInputSampler,UV+float2(0,t.y)).rgb,float3(.2126,.7152,.0722))-dot(Texture2DSample(CyInput,CyInputSampler,UV-float2(0,t.y)).rgb,float3(.2126,.7152,.0722));float edge=pow(saturate(length(float2(lx,ly))*EdgeStrength),EdgePower);float grain=frac(sin(dot(floor(UV*GrainScale),float2(12.9898,78.233)))*43758.5453);float shade=saturate(1.0-edge-(1.0-l)*Shading-(grain-.5)*GrainAmount);float3 mono=lerp(PencilColor.rgb,PaperColor.rgb,shade);float3 outColor=lerp(mono,mono*c.rgb*1.5,PreserveColor);return float4(lerp(c.rgb,outColor,Strength),c.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, EdgeStrength, EdgePower, Shading, GrainScale, GrainAmount, PreserveColor, Strength, PencilColor.rgb, PaperColor.rgb);
}
