//@name Edge Wear
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Edge Wear","nextId":17,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":32,"min":0.25,"name":"Radius","op":"param","pos":[40, 330],"text":"Radius","value":[2, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":128,"min":0,"name":"EdgeStrength","op":"param","pos":[40, 440],"text":"Edge Strength","value":[8, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":8,"min":0.0500000007,"name":"EdgePower","op":"param","pos":[40, 550],"text":"Edge Power","value":[0.800000012, 0, 0, 0]},{"id":7,"in":[],"index":1,"max":4096,"min":1,"name":"NoiseScale","op":"param","pos":[40, 660],"text":"Noise Scale","value":[180, 0, 0, 0]},{"id":8,"in":[],"index":1,"max":1000,"min":-1000,"name":"Seed","op":"param","pos":[40, 770],"text":"Seed","value":[0, 0, 0, 0]},{"id":9,"in":[],"index":1,"name":"NoiseThreshold","op":"param","pos":[40, 880],"text":"Noise Threshold","value":[0.180000007, 0, 0, 0]},{"id":10,"in":[],"index":1,"max":32,"min":0,"name":"NoiseContrast","op":"param","pos":[40, 990],"text":"Noise Contrast","value":[2.5, 0, 0, 0]},{"id":11,"in":[],"index":1,"name":"HeightMinimum","op":"param","pos":[40, 1100],"text":"Height Minimum","value":[0, 0, 0, 0]},{"id":12,"in":[],"index":1,"name":"HeightMaximum","op":"param","pos":[40, 1210],"text":"Height Maximum","value":[1, 0, 0, 0]},{"id":13,"in":[],"index":1,"name":"Invert","op":"param","pos":[40, 1320],"text":"Invert","value":[0, 0, 0, 0]},{"id":14,"in":[],"index":1,"name":"HeightChannel","op":"paramcolor","pos":[40, 1430],"text":"Height Channel","value":[1, 0, 0, 0]},{"id":15,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0],[11, 0],[12, 0],[13, 0],[14, 0]],"name":"Edge Wear","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Radius",1],["EdgeStrength",1],["EdgePower",1],["NoiseScale",1],["Seed",1],["NoiseThreshold",1],["NoiseContrast",1],["HeightMinimum",1],["HeightMaximum",1],["Invert",1],["HeightChannel",3]],"pos":[420, 40],"text":"    float2 t = Radius / max(CyCanvasSize.xy, 1.0);\n    float h0 = dot(Texture2DSample(CyInput, CyInputSampler, UV).rgb, HeightChannel.rgb);\n    float hx = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(t.x, 0)).rgb, HeightChannel.rgb) - dot(Texture2DSample(CyInput, CyInputSampler, UV - float2(t.x, 0)).rgb, HeightChannel.rgb);\n    float hy = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(0, t.y)).rgb, HeightChannel.rgb) - dot(Texture2DSample(CyInput, CyInputSampler, UV - float2(0, t.y)).rgb, HeightChannel.rgb);\n    float edge = pow(saturate(length(float2(hx, hy)) * EdgeStrength), EdgePower);\n    float n = frac(sin(dot(floor(UV * NoiseScale) + Seed, float2(12.9898, 78.233))) * 43758.5453);\n    float wear = edge * saturate((n - NoiseThreshold) * NoiseContrast) * smoothstep(HeightMinimum, HeightMaximum, h0);\n    wear = lerp(wear, 1.0 - wear, Invert);\n    return float4(wear.xxx, 1);","value":[0, 0, 0, 0]},{"id":16,"in":[[15, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo full
//@param Radius float 2 0.25 32 "Radius"
//@param EdgeStrength float 8 0 128 "Edge Strength"
//@param EdgePower float 0.8 0.05 8 "Edge Power"
//@param NoiseScale float 180 1 4096 "Noise Scale"
//@param Seed float 0 -1000 1000 "Seed"
//@param NoiseThreshold float 0.18 0 1 "Noise Threshold"
//@param NoiseContrast float 2.5 0 32 "Noise Contrast"
//@param HeightMinimum float 0 0 1 "Height Minimum"
//@param HeightMaximum float 1 0 1 "Height Maximum"
//@param Invert float 0 0 1 "Invert"
//@param HeightChannel color 1 0 0 0 "Height Channel"
// Imported preset (Effect / PBR): Builds a controllable worn-edge mask from source height or luminance with procedural breakup.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float EdgeStrength, float EdgePower, float NoiseScale, float Seed, float NoiseThreshold, float NoiseContrast, float HeightMinimum, float HeightMaximum, float Invert, float3 HeightChannel)
{
    float2 t = Radius / max(CyCanvasSize.xy, 1.0);
    float h0 = dot(Texture2DSample(CyInput, CyInputSampler, UV).rgb, HeightChannel.rgb);
    float hx = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(t.x, 0)).rgb, HeightChannel.rgb) - dot(Texture2DSample(CyInput, CyInputSampler, UV - float2(t.x, 0)).rgb, HeightChannel.rgb);
    float hy = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(0, t.y)).rgb, HeightChannel.rgb) - dot(Texture2DSample(CyInput, CyInputSampler, UV - float2(0, t.y)).rgb, HeightChannel.rgb);
    float edge = pow(saturate(length(float2(hx, hy)) * EdgeStrength), EdgePower);
    float n = frac(sin(dot(floor(UV * NoiseScale) + Seed, float2(12.9898, 78.233))) * 43758.5453);
    float wear = edge * saturate((n - NoiseThreshold) * NoiseContrast) * smoothstep(HeightMinimum, HeightMaximum, h0);
    wear = lerp(wear, 1.0 - wear, Invert);
    return float4(wear.xxx, 1);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, EdgeStrength, EdgePower, NoiseScale, Seed, NoiseThreshold, NoiseContrast, HeightMinimum, HeightMaximum, Invert, HeightChannel.rgb);
}
