//@name Height Erosion
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Height Erosion","nextId":16,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":64,"min":0.25,"name":"Radius","op":"param","pos":[40, 330],"text":"Radius","value":[2, 0, 0, 0]},{"id":5,"in":[],"index":1,"name":"Talus","op":"param","pos":[40, 440],"text":"Talus","value":[0.0149999997, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":128,"min":0,"name":"Strength","op":"param","pos":[40, 550],"text":"Strength","value":[12, 0, 0, 0]},{"id":7,"in":[],"index":1,"name":"Erosion","op":"param","pos":[40, 660],"text":"Erosion","value":[0.649999976, 0, 0, 0]},{"id":8,"in":[],"index":1,"name":"Deposition","op":"param","pos":[40, 770],"text":"Deposition","value":[0.180000007, 0, 0, 0]},{"id":9,"in":[],"index":1,"max":4096,"min":1,"name":"NoiseScale","op":"param","pos":[40, 880],"text":"Noise Scale","value":[128, 0, 0, 0]},{"id":10,"in":[],"index":1,"max":1000,"min":-1000,"name":"Seed","op":"param","pos":[40, 990],"text":"Seed","value":[0, 0, 0, 0]},{"id":11,"in":[],"index":1,"name":"NoiseAmount","op":"param","pos":[40, 1100],"text":"Noise Amount","value":[0.349999994, 0, 0, 0]},{"id":12,"in":[],"index":1,"name":"HeightChannel","op":"paramcolor","pos":[40, 1210],"text":"Height Channel","value":[1, 0, 0, 0]},{"id":13,"in":[],"index":1,"name":"ChannelMask","op":"paramcolor","pos":[40, 1320],"text":"Channel Mask","value":[1, 1, 1, 0]},{"id":14,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0],[11, 0],[12, 0],[13, 0]],"name":"Height Erosion","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Radius",1],["Talus",1],["Strength",1],["Erosion",1],["Deposition",1],["NoiseScale",1],["Seed",1],["NoiseAmount",1],["HeightChannel",3],["ChannelMask",3]],"pos":[420, 40],"text":"    float2 t = Radius / max(CyCanvasSize.xy, 1.0);\n    float4 src = Texture2DSample(CyInput, CyInputSampler, UV);\n    float h = dot(src.rgb, HeightChannel.rgb);\n    float lower = h, average = 0;\n    [unroll] for (int i = 0; i < 16; i++) {\n        float a = i * 6.283185 / 16.0;\n        float sampleH = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(cos(a), sin(a)) * t).rgb, HeightChannel.rgb);\n        lower = min(lower, sampleH);\n        average += sampleH;\n    }\n    average /= 16;\n    float slope = saturate((h - lower - Talus) * Strength);\n    float noise = frac(sin(dot(floor(UV * NoiseScale) + Seed, float2(12.9898, 78.233))) * 43758.5453);\n    float eroded = lerp(h, lower, slope * Erosion * lerp(1.0 - NoiseAmount, 1.0, noise));\n    eroded = lerp(eroded, average, Deposition * (1.0 - slope));\n    float3 outColor = lerp(src.rgb, eroded.xxx, ChannelMask.rgb);\n    return float4(outColor, src.a);","value":[0, 0, 0, 0]},{"id":15,"in":[[14, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo full
//@param Radius float 2 0.25 64 "Radius"
//@param Talus float 0.015 0 1 "Talus"
//@param Strength float 12 0 128 "Strength"
//@param Erosion float 0.65 0 1 "Erosion"
//@param Deposition float 0.18 0 1 "Deposition"
//@param NoiseScale float 128 1 4096 "Noise Scale"
//@param Seed float 0 -1000 1000 "Seed"
//@param NoiseAmount float 0.35 0 1 "Noise Amount"
//@param HeightChannel color 1 0 0 0 "Height Channel"
//@param ChannelMask color 1 1 1 0 "Channel Mask"
// Imported preset (Effect / PBR): Iterative-looking height erosion approximation with talus, deposition, and noise-driven weathering.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float Talus, float Strength, float Erosion, float Deposition, float NoiseScale, float Seed, float NoiseAmount, float3 HeightChannel, float3 ChannelMask)
{
    float2 t = Radius / max(CyCanvasSize.xy, 1.0);
    float4 src = Texture2DSample(CyInput, CyInputSampler, UV);
    float h = dot(src.rgb, HeightChannel.rgb);
    float lower = h, average = 0;
    [unroll] for (int i = 0; i < 16; i++) {
        float a = i * 6.283185 / 16.0;
        float sampleH = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(cos(a), sin(a)) * t).rgb, HeightChannel.rgb);
        lower = min(lower, sampleH);
        average += sampleH;
    }
    average /= 16;
    float slope = saturate((h - lower - Talus) * Strength);
    float noise = frac(sin(dot(floor(UV * NoiseScale) + Seed, float2(12.9898, 78.233))) * 43758.5453);
    float eroded = lerp(h, lower, slope * Erosion * lerp(1.0 - NoiseAmount, 1.0, noise));
    eroded = lerp(eroded, average, Deposition * (1.0 - slope));
    float3 outColor = lerp(src.rgb, eroded.xxx, ChannelMask.rgb);
    return float4(outColor, src.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, Talus, Strength, Erosion, Deposition, NoiseScale, Seed, NoiseAmount, HeightChannel.rgb, ChannelMask.rgb);
}
