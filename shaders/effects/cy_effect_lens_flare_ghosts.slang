//@name Lens Flare Ghosts
//@category Traitements
//@graph {"name":"Lens Flare Ghosts","category":"Traitements","inputs":1,"premultiplied":false,"nextId":13,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[1.20000005,0.0,0.0,0.0],"name":"GhostScale","text":"Ghost Scale","index":1,"min":0.0500000007,"max":8.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.75,0.0,0.0,0.0],"name":"Threshold","text":"Threshold","index":1,"min":0.0,"max":16.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.25,0.0,0.0,0.0],"name":"Softness","text":"Softness","index":1,"min":0.00100000005,"max":4.0},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[1.5,0.0,0.0,0.0],"name":"Intensity","text":"Intensity","index":1,"min":0.0,"max":32.0},{"id":8,"op":"param2","pos":[40,770],"in":[],"value":[0.5,0.5,0.0,0.0],"name":"Center","text":"Center","index":1},{"id":9,"op":"paramcolor","pos":[40,880],"in":[],"value":[1.0,0.200000003,0.0500000007,1.0],"name":"GhostColorA","text":"Ghost Color A","index":1},{"id":10,"op":"paramcolor","pos":[40,990],"in":[],"value":[0.0500000007,0.300000012,1.0,1.0],"name":"GhostColorB","text":"Ghost Color B","index":1},{"id":11,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0]],"value":[0,0,0,0],"name":"Lens Flare Ghosts","text":"    float4 src=Texture2DSample(CyInput,CyInputSampler,UV); float2 p=UV-Center.xy; float2 g1=saturate(Center.xy-p*GhostScale); float2 g2=saturate(Center.xy+p*GhostScale*0.55); float3 a=Texture2DSample(CyInput,CyInputSampler,g1).rgb; float3 b=Texture2DSample(CyInput,CyInputSampler,g2).rgb; float la=dot(a,float3(.2126,.7152,.0722)); float lb=dot(b,float3(.2126,.7152,.0722)); a*=saturate((la-Threshold)/max(Softness,0.001)); b*=saturate((lb-Threshold)/max(Softness,0.001)); float3 ghosts=(a*GhostColorA.rgb+b*GhostColorB.rgb)*Intensity; return float4(src.rgb+ghosts,src.a);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["GhostScale",1],["Threshold",1],["Softness",1],["Intensity",1],["Center",3],["GhostColorA",3],["GhostColorB",3]],"outs":[["Resultat",4]]},{"id":12,"op":"output","pos":[780,40],"in":[[11,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo full
//@param GhostScale float 1.2 0.05 8 "Ghost Scale"
//@param Threshold float 0.75 0 16 "Threshold"
//@param Softness float 0.25 0.001 4 "Softness"
//@param Intensity float 1.5 0 32 "Intensity"
//@param Center float2 0.5 0.5 0 1 "Center"
//@param GhostColorA color 1 0.2 0.05 1 "Ghost Color A"
//@param GhostColorB color 0.05 0.3 1 1 "Ghost Color B"

// Imported preset (Effect / Color & Tone): Extracts bright regions into mirrored chromatic lens ghosts.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float GhostScale, float Threshold, float Softness, float Intensity, float3 Center, float3 GhostColorA, float3 GhostColorB)
{
    float4 src=Texture2DSample(CyInput,CyInputSampler,UV); float2 p=UV-Center.xy; float2 g1=saturate(Center.xy-p*GhostScale); float2 g2=saturate(Center.xy+p*GhostScale*0.55); float3 a=Texture2DSample(CyInput,CyInputSampler,g1).rgb; float3 b=Texture2DSample(CyInput,CyInputSampler,g2).rgb; float la=dot(a,float3(.2126,.7152,.0722)); float lb=dot(b,float3(.2126,.7152,.0722)); a*=saturate((la-Threshold)/max(Softness,0.001)); b*=saturate((lb-Threshold)/max(Softness,0.001)); float3 ghosts=(a*GhostColorA.rgb+b*GhostColorB.rgb)*Intensity; return float4(src.rgb+ghosts,src.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), GhostScale, Threshold, Softness, Intensity, float3(Center, 0.0), GhostColorA.rgb, GhostColorB.rgb);
}
