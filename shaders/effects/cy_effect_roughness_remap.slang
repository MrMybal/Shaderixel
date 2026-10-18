//@name Roughness Remap
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Roughness Remap","nextId":17,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"name":"InputMinimum","op":"param","pos":[40, 330],"text":"Input Minimum","value":[0, 0, 0, 0]},{"id":5,"in":[],"index":1,"name":"InputMaximum","op":"param","pos":[40, 440],"text":"Input Maximum","value":[1, 0, 0, 0]},{"id":6,"in":[],"index":1,"name":"OutputMinimum","op":"param","pos":[40, 550],"text":"Output Minimum","value":[0.0399999991, 0, 0, 0]},{"id":7,"in":[],"index":1,"name":"OutputMaximum","op":"param","pos":[40, 660],"text":"Output Maximum","value":[1, 0, 0, 0]},{"id":8,"in":[],"index":1,"max":8,"min":0.00999999978,"name":"Gamma","op":"param","pos":[40, 770],"text":"Gamma","value":[1, 0, 0, 0]},{"id":9,"in":[],"index":1,"name":"PhysicalMinimum","op":"param","pos":[40, 880],"text":"Physical Minimum","value":[0.0199999996, 0, 0, 0]},{"id":10,"in":[],"index":1,"max":4096,"min":1,"name":"MicroScale","op":"param","pos":[40, 990],"text":"Micro Scale","value":[256, 0, 0, 0]},{"id":11,"in":[],"index":1,"max":1000,"min":-1000,"name":"Seed","op":"param","pos":[40, 1100],"text":"Seed","value":[0, 0, 0, 0]},{"id":12,"in":[],"index":1,"max":0.5,"min":0,"name":"MicroVariation","op":"param","pos":[40, 1210],"text":"Micro Variation","value":[0.0299999993, 0, 0, 0]},{"id":13,"in":[],"index":1,"name":"SourceChannel","op":"paramcolor","pos":[40, 1320],"text":"Source Channel","value":[1, 0, 0, 0]},{"id":14,"in":[],"index":1,"name":"OutputChannels","op":"paramcolor","pos":[40, 1430],"text":"Output Channels","value":[1, 1, 1, 0]},{"id":15,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0],[11, 0],[12, 0],[13, 0],[14, 0]],"name":"Roughness Remap","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["InputMinimum",1],["InputMaximum",1],["OutputMinimum",1],["OutputMaximum",1],["Gamma",1],["PhysicalMinimum",1],["MicroScale",1],["Seed",1],["MicroVariation",1],["SourceChannel",3],["OutputChannels",3]],"pos":[420, 40],"text":"    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);\n    float rough = dot(s.rgb, SourceChannel.rgb);\n    rough = saturate((rough - InputMinimum) / max(InputMaximum - InputMinimum, .0001));\n    rough = pow(rough, max(Gamma, .01));\n    rough = lerp(OutputMinimum, OutputMaximum, rough);\n    float n = frac(sin(dot(floor(UV * MicroScale) + Seed, float2(12.9898, 78.233))) * 43758.5453) - .5;\n    rough = clamp(rough + n * MicroVariation, PhysicalMinimum, 1.0);\n    float3 outColor = lerp(s.rgb, rough.xxx, OutputChannels.rgb);\n    return float4(outColor, s.a);","value":[0, 0, 0, 0]},{"id":16,"in":[[15, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
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
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float InputMinimum, float InputMaximum, float OutputMinimum, float OutputMaximum, float Gamma, float PhysicalMinimum, float MicroScale, float Seed, float MicroVariation, float3 SourceChannel, float3 OutputChannels)
{
    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);
    float rough = dot(s.rgb, SourceChannel.rgb);
    rough = saturate((rough - InputMinimum) / max(InputMaximum - InputMinimum, .0001));
    rough = pow(rough, max(Gamma, .01));
    rough = lerp(OutputMinimum, OutputMaximum, rough);
    float n = frac(sin(dot(floor(UV * MicroScale) + Seed, float2(12.9898, 78.233))) * 43758.5453) - .5;
    rough = clamp(rough + n * MicroVariation, PhysicalMinimum, 1.0);
    float3 outColor = lerp(s.rgb, rough.xxx, OutputChannels.rgb);
    return float4(outColor, s.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), InputMinimum, InputMaximum, OutputMinimum, OutputMaximum, Gamma, PhysicalMinimum, MicroScale, Seed, MicroVariation, SourceChannel.rgb, OutputChannels.rgb);
}
