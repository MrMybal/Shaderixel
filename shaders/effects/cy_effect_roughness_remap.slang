//@name Roughness Remap
//@category Traitements
//@graph {"name":"Roughness Remap","category":"Traitements","inputs":1,"premultiplied":false,"nextId":17,"nodes":[{"id":1,"op":"texture","pos":[40,40],"in":[],"value":[0,0,0,0],"index":0,"name":"CyInput"},{"id":2,"op":"uv","pos":[40,150],"in":[],"value":[0,0,0,0]},{"id":3,"op":"canvassize","pos":[40,240],"in":[],"value":[0,0,0,0]},{"id":4,"op":"param","pos":[40,330],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"InputMinimum","text":"Input Minimum","index":1},{"id":5,"op":"param","pos":[40,440],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"InputMaximum","text":"Input Maximum","index":1},{"id":6,"op":"param","pos":[40,550],"in":[],"value":[0.0399999991,0.0,0.0,0.0],"name":"OutputMinimum","text":"Output Minimum","index":1},{"id":7,"op":"param","pos":[40,660],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"OutputMaximum","text":"Output Maximum","index":1},{"id":8,"op":"param","pos":[40,770],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"Gamma","text":"Gamma","index":1,"min":0.00999999978,"max":8.0},{"id":9,"op":"param","pos":[40,880],"in":[],"value":[0.0199999996,0.0,0.0,0.0],"name":"PhysicalMinimum","text":"Physical Minimum","index":1},{"id":10,"op":"param","pos":[40,990],"in":[],"value":[256.0,0.0,0.0,0.0],"name":"MicroScale","text":"Micro Scale","index":1,"min":1.0,"max":4096.0},{"id":11,"op":"param","pos":[40,1100],"in":[],"value":[0.0,0.0,0.0,0.0],"name":"Seed","text":"Seed","index":1,"min":-1000.0,"max":1000.0},{"id":12,"op":"param","pos":[40,1210],"in":[],"value":[0.0299999993,0.0,0.0,0.0],"name":"MicroVariation","text":"Micro Variation","index":1,"min":0.0,"max":0.5},{"id":13,"op":"paramcolor","pos":[40,1320],"in":[],"value":[1.0,0.0,0.0,0.0],"name":"SourceChannel","text":"Source Channel","index":1},{"id":14,"op":"paramcolor","pos":[40,1430],"in":[],"value":[1.0,1.0,1.0,0.0],"name":"OutputChannels","text":"Output Channels","index":1},{"id":15,"op":"custom","pos":[420,40],"in":[[1,0],[2,0],[3,0],[4,0],[5,0],[6,0],[7,0],[8,0],[9,0],[10,0],[11,0],[12,0],[13,0],[14,0]],"value":[0,0,0,0],"name":"Roughness Remap","text":"    float4 s=Texture2DSample(CyInput,CyInputSampler,UV);float rough=dot(s.rgb,SourceChannel.rgb);rough=saturate((rough-InputMinimum)/max(InputMaximum-InputMinimum,.0001));rough=pow(rough,max(Gamma,.01));rough=lerp(OutputMinimum,OutputMaximum,rough);float n=frac(sin(dot(floor(UV*MicroScale)+Seed,float2(12.9898,78.233)))*43758.5453)-.5;rough=clamp(rough+n*MicroVariation,PhysicalMinimum,1.0);float3 outColor=lerp(s.rgb,rough.xxx,OutputChannels.rgb);return float4(outColor,s.a);","pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["InputMinimum",1],["InputMaximum",1],["OutputMinimum",1],["OutputMaximum",1],["Gamma",1],["PhysicalMinimum",1],["MicroScale",1],["Seed",1],["MicroVariation",1],["SourceChannel",3],["OutputChannels",3]],"outs":[["Resultat",4]]},{"id":16,"op":"output","pos":[780,40],"in":[[15,0],[0,0],[0,0]],"value":[0,0,0,0]}]}
//@input Image
//@halo 0
//@param InputMinimum float 0 0 1 "Input Minimum"
//@param InputMaximum float 1 0 1 "Input Maximum"
//@param OutputMinimum float 0.04 0 1 "Output Minimum"
//@param OutputMaximum float 1 0 1 "Output Maximum"
//@param Gamma float 1 0.01 8 "Gamma"
//@param PhysicalMinimum float 0.02 0 1 "Physical Minimum"
//@param MicroScale float 256 1 4096 "Micro Scale"
//@param Seed float 0 -1000 1000 "Seed"
//@param MicroVariation float 0.03 0 0.5 "Micro Variation"
//@param SourceChannel color 1 0 0 0 "Source Channel"
//@param OutputChannels color 1 1 1 0 "Output Channels"

// Imported preset (Effect / PBR): PBR roughness remap with range compression, gamma, microvariation, and physical clamping.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) { return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0); }
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;

float4 CyPreset(float2 UV, float4 CyCanvasSize, float InputMinimum, float InputMaximum, float OutputMinimum, float OutputMaximum, float Gamma, float PhysicalMinimum, float MicroScale, float Seed, float MicroVariation, float3 SourceChannel, float3 OutputChannels)
{
    float4 s=Texture2DSample(CyInput,CyInputSampler,UV);float rough=dot(s.rgb,SourceChannel.rgb);rough=saturate((rough-InputMinimum)/max(InputMaximum-InputMinimum,.0001));rough=pow(rough,max(Gamma,.01));rough=lerp(OutputMinimum,OutputMaximum,rough);float n=frac(sin(dot(floor(UV*MicroScale)+Seed,float2(12.9898,78.233)))*43758.5453)-.5;rough=clamp(rough+n*MicroVariation,PhysicalMinimum,1.0);float3 outColor=lerp(s.rgb,rough.xxx,OutputChannels.rgb);return float4(outColor,s.a);
}

float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), InputMinimum, InputMaximum, OutputMinimum, OutputMaximum, Gamma, PhysicalMinimum, MicroScale, Seed, MicroVariation, SourceChannel.rgb, OutputChannels.rgb);
}
