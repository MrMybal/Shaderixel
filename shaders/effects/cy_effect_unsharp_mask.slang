//@name Unsharp Mask
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Unsharp Mask","nextId":14,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":128,"min":0.25,"name":"Radius","op":"param","pos":[40, 330],"text":"Radius","value":[3, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":8,"min":0,"name":"Amount","op":"param","pos":[40, 440],"text":"Amount","value":[1, 0, 0, 0]},{"id":6,"in":[],"index":1,"name":"Threshold","op":"param","pos":[40, 550],"text":"Threshold","value":[0.0199999996, 0, 0, 0]},{"id":7,"in":[],"index":1,"max":128,"min":1,"name":"Steps","op":"param","pos":[40, 660],"text":"Steps","value":[20, 0, 0, 0]},{"id":8,"in":[],"index":1,"name":"HighQuality","op":"param","pos":[40, 770],"text":"High Quality","value":[0, 0, 0, 0]},{"id":9,"in":[],"index":1,"name":"Dither","op":"param","pos":[40, 880],"text":"Dither","value":[1, 0, 0, 0]},{"id":10,"in":[],"index":1,"max":2,"min":0,"name":"DitherAmount","op":"param","pos":[40, 990],"text":"Dither Amount","value":[0.75, 0, 0, 0]},{"id":11,"in":[],"index":1,"max":1000,"min":-1000,"name":"DitherSeed","op":"param","pos":[40, 1100],"text":"Dither Seed","value":[0, 0, 0, 0]},{"id":12,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0],[11, 0]],"name":"Unsharp Mask","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Radius",1],["Amount",1],["Threshold",1],["Steps",1],["HighQuality",1],["Dither",1],["DitherAmount",1],["DitherSeed",1]],"pos":[420, 40],"text":"    int Count = HighQuality > 0.5 ? 128 : clamp((int)round(Steps), 1, 128);\n    float2 DitherPixel = floor(UV * max(CyCanvasSize.xy, 1.0));\n    float4 src = Texture2DSample(CyInput, CyInputSampler, UV);\n    float4 avg = 0;\n    [loop] for (int Index = 0; Index < 128; ++Index) {\n        if (Index >= Count) break;\n        float Jitter = (frac(sin(dot(DitherPixel + float2(Index * 17.17 + DitherSeed, Index * 7.13 - DitherSeed), float2(12.9898, 78.233))) * 43758.5453) - .5) * 2.0 * DitherAmount * Dither;\n        float f = saturate((Index + 0.5 + Jitter) / Count);\n        float a = Index * 2.39996323;\n        float2 o = float2(cos(a), sin(a)) * sqrt(f) * Radius / max(CyCanvasSize.xy, 1.0);\n        avg += Texture2DSample(CyInput, CyInputSampler, UV + o);\n    }\n    avg /= max((float)Count, 1.0);\n    float3 detail = src.rgb - avg.rgb;\n    detail *= step(Threshold, abs(detail));\n    return float4(src.rgb + detail * Amount, src.a);","value":[0, 0, 0, 0]},{"id":13,"in":[[12, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo full
//@param Radius float 3 0.25 128 "Radius"
//@param Amount float 1 0 8 "Amount"
//@param Threshold float 0.02 0 1 "Threshold"
//@param Steps float 20 1 128 "Steps"
//@param HighQuality float 0 0 1 "High Quality"
//@param Dither float 1 0 1 "Dither"
//@param DitherAmount float 0.75 0 2 "Dither Amount"
//@param DitherSeed float 0 -1000 1000 "Dither Seed"
// Imported preset (Effect / Detail): Non-destructive unsharp mask with threshold and quality-controlled blur.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float Amount, float Threshold, float Steps, float HighQuality, float Dither, float DitherAmount, float DitherSeed)
{
    int Count = HighQuality > 0.5 ? 128 : clamp((int)round(Steps), 1, 128);
    float2 DitherPixel = floor(UV * max(CyCanvasSize.xy, 1.0));
    float4 src = Texture2DSample(CyInput, CyInputSampler, UV);
    float4 avg = 0;
    [loop] for (int Index = 0; Index < 128; ++Index) {
        if (Index >= Count) break;
        float Jitter = (frac(sin(dot(DitherPixel + float2(Index * 17.17 + DitherSeed, Index * 7.13 - DitherSeed), float2(12.9898, 78.233))) * 43758.5453) - .5) * 2.0 * DitherAmount * Dither;
        float f = saturate((Index + 0.5 + Jitter) / Count);
        float a = Index * 2.39996323;
        float2 o = float2(cos(a), sin(a)) * sqrt(f) * Radius / max(CyCanvasSize.xy, 1.0);
        avg += Texture2DSample(CyInput, CyInputSampler, UV + o);
    }
    avg /= max((float)Count, 1.0);
    float3 detail = src.rgb - avg.rgb;
    detail *= step(Threshold, abs(detail));
    return float4(src.rgb + detail * Amount, src.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, Amount, Threshold, Steps, HighQuality, Dither, DitherAmount, DitherSeed);
}
