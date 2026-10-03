//@name Noise
//@category Masques
//@graph {"name":"Noise","category":"Masques","inputs":1,"premultiplied":false,"nextId":11,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[256.0,0.0,0.0,0.0],"name":"Scale","text":"Scale","index":1,"min":1.0,"max":4096.0},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Seed","text":"Seed","index":1,"min":-1000.0,"max":1000.0},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.349999994,0.0,0.0,0.0],"name":"Threshold","text":"Threshold","index":1},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[4.0,0.0,0.0,0.0],"name":"Contrast","text":"Contrast","index":1,"min":0.0,"max":32.0},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"UseSource","text":"Use Source","index":1},{"id":9,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0]],"value":[0,0,0,0],"name":"Noise","text":"    float l=dot(Texture2DSample(CyInput,CyInputSampler,UV).rgb,float3(.2126,.7152,.0722)); float n=frac(sin(dot(floor(UV*max(Scale,1.0))+Seed,float2(12.9898,78.233)))*43758.5453); float v=saturate((n-Threshold)*Contrast)*lerp(1.0,l,saturate(UseSource)); return float4(v.xxx,1);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Scale",1],["Seed",1],["Threshold",1],["Contrast",1],["UseSource",1]],"outs":[["Resultat",4]]},{"id":10,"op":"output","pos":[780,40],"in":[[9,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo 0
//@param Scale float 256 1 4096 "Scale"
//@param Seed float 0 -1000 1000 "Seed"
//@param Threshold float 0.35 0 1 "Threshold"
//@param Contrast float 4 0 32 "Contrast"
//@param UseSource float 0 0 1 "Use Source"

// Imported preset (Mask / Mask): Multiplies source luminance by procedural noise.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float Scale, float Seed, float Threshold, float Contrast, float UseSource)
{
    float l=dot(Texture2DSample(CyInput,CyInputSampler,UV).rgb,float3(.2126,.7152,.0722)); float n=frac(sin(dot(floor(UV*max(Scale,1.0))+Seed,float2(12.9898,78.233)))*43758.5453); float v=saturate((n-Threshold)*Contrast)*lerp(1.0,l,saturate(UseSource)); return float4(v.xxx,1);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Scale, Seed, Threshold, Contrast, UseSource);
}
