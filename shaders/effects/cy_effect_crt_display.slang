//@name CRT Display
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"CRT Display","nextId":13,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":0.75,"min":0,"name":"Curvature","op":"param","pos":[40, 330],"text":"Curvature","value":[0.0799999982, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":32,"min":0,"name":"ChromaticOffset","op":"param","pos":[40, 440],"text":"Chromatic Offset","value":[1.5, 0, 0, 0]},{"id":6,"in":[],"index":1,"name":"ScanlineAmount","op":"param","pos":[40, 550],"text":"Scanline Amount","value":[0.349999994, 0, 0, 0]},{"id":7,"in":[],"index":1,"name":"PhosphorAmount","op":"param","pos":[40, 660],"text":"Phosphor Amount","value":[0.300000012, 0, 0, 0]},{"id":8,"in":[],"index":1,"max":2,"min":0,"name":"Vignette","op":"param","pos":[40, 770],"text":"Vignette","value":[0.319999993, 0, 0, 0]},{"id":9,"in":[],"index":1,"max":8,"min":0.0500000007,"name":"VignettePower","op":"param","pos":[40, 880],"text":"Vignette Power","value":[1.39999998, 0, 0, 0]},{"id":10,"in":[],"index":1,"max":8,"min":0,"name":"Brightness","op":"param","pos":[40, 990],"text":"Brightness","value":[1.14999998, 0, 0, 0]},{"id":11,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0]],"name":"CRT Display","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Curvature",1],["ChromaticOffset",1],["ScanlineAmount",1],["PhosphorAmount",1],["Vignette",1],["VignettePower",1],["Brightness",1]],"pos":[420, 40],"text":"    float2 p = UV * 2.0 - 1.0;\n    float2 curved = p * (1.0 + Curvature * dot(p, p));\n    float2 uv = curved * .5 + .5;\n    float2 px = float2(ChromaticOffset / max(CyCanvasSize.x, 1.0), 0);\n    float r = Texture2DSample(CyInput, CyInputSampler, uv + px).r;\n    float g = Texture2DSample(CyInput, CyInputSampler, uv).g;\n    float b = Texture2DSample(CyInput, CyInputSampler, uv - px).b;\n    float scan = lerp(1.0, 0.5 + 0.5 * sin(uv.y * CyCanvasSize.y * 3.14159), ScanlineAmount);\n    float3 triad = lerp(1.0, 0.72 + 0.28 * cos(uv.x * CyCanvasSize.x * 2.0944 + float3(0, 2.0944, 4.1888)), PhosphorAmount);\n    float vign = pow(saturate(1.0 - dot(p, p) * Vignette), VignettePower);\n    float inside = step(0.0, uv.x) * step(uv.x, 1.0) * step(0.0, uv.y) * step(uv.y, 1.0);\n    float3 v = float3(r, g, b) * scan * triad * vign * Brightness * inside;\n    float a = Texture2DSample(CyInput, CyInputSampler, saturate(uv)).a * inside;\n    return float4(v, a);","value":[0, 0, 0, 0]},{"id":12,"in":[[11, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo full
//@param Curvature float 0.08 0 0.75 "Curvature"
//@param ChromaticOffset float 1.5 0 32 "Chromatic Offset"
//@param ScanlineAmount float 0.35 0 1 "Scanline Amount"
//@param PhosphorAmount float 0.3 0 1 "Phosphor Amount"
//@param Vignette float 0.32 0 2 "Vignette"
//@param VignettePower float 1.4 0.05 8 "Vignette Power"
//@param Brightness float 1.15 0 8 "Brightness"
// Imported preset (Effect / Stylize): Curved CRT display simulation with RGB phosphors, scanlines, vignette, and chromatic convergence.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Curvature, float ChromaticOffset, float ScanlineAmount, float PhosphorAmount, float Vignette, float VignettePower, float Brightness)
{
    float2 p = UV * 2.0 - 1.0;
    float2 curved = p * (1.0 + Curvature * dot(p, p));
    float2 uv = curved * .5 + .5;
    float2 px = float2(ChromaticOffset / max(CyCanvasSize.x, 1.0), 0);
    float r = Texture2DSample(CyInput, CyInputSampler, uv + px).r;
    float g = Texture2DSample(CyInput, CyInputSampler, uv).g;
    float b = Texture2DSample(CyInput, CyInputSampler, uv - px).b;
    float scan = lerp(1.0, 0.5 + 0.5 * sin(uv.y * CyCanvasSize.y * 3.14159), ScanlineAmount);
    float3 triad = lerp(1.0, 0.72 + 0.28 * cos(uv.x * CyCanvasSize.x * 2.0944 + float3(0, 2.0944, 4.1888)), PhosphorAmount);
    float vign = pow(saturate(1.0 - dot(p, p) * Vignette), VignettePower);
    float inside = step(0.0, uv.x) * step(uv.x, 1.0) * step(0.0, uv.y) * step(uv.y, 1.0);
    float3 v = float3(r, g, b) * scan * triad * vign * Brightness * inside;
    float a = Texture2DSample(CyInput, CyInputSampler, saturate(uv)).a * inside;
    return float4(v, a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Curvature, ChromaticOffset, ScanlineAmount, PhosphorAmount, Vignette, VignettePower, Brightness);
}
