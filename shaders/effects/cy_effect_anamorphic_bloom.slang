//@name Anamorphic Bloom
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Anamorphic Bloom","nextId":16,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":16,"min":0,"name":"Threshold","op":"param","pos":[40, 330],"text":"Threshold","value":[1, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":1024,"min":0.25,"name":"Radius","op":"param","pos":[40, 440],"text":"Radius","value":[48, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":16,"min":0.00999999978,"name":"Falloff","op":"param","pos":[40, 550],"text":"Falloff","value":[2, 0, 0, 0]},{"id":7,"in":[],"index":1,"max":64,"min":0,"name":"Intensity","op":"param","pos":[40, 660],"text":"Intensity","value":[2, 0, 0, 0]},{"id":8,"in":[],"index":1,"max":128,"min":1,"name":"Steps","op":"param","pos":[40, 770],"text":"Steps","value":[32, 0, 0, 0]},{"id":9,"in":[],"index":1,"name":"HighQuality","op":"param","pos":[40, 880],"text":"High Quality","value":[0, 0, 0, 0]},{"id":10,"in":[],"index":1,"name":"Dither","op":"param","pos":[40, 990],"text":"Dither","value":[1, 0, 0, 0]},{"id":11,"in":[],"index":1,"max":2,"min":0,"name":"DitherAmount","op":"param","pos":[40, 1100],"text":"Dither Amount","value":[0.75, 0, 0, 0]},{"id":12,"in":[],"index":1,"max":1000,"min":-1000,"name":"DitherSeed","op":"param","pos":[40, 1210],"text":"Dither Seed","value":[0, 0, 0, 0]},{"id":13,"in":[],"index":1,"name":"Tint","op":"paramcolor","pos":[40, 1320],"text":"Tint","value":[0.200000003, 0.449999988, 1, 1]},{"id":14,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0],[11, 0],[12, 0],[13, 0]],"name":"Anamorphic Bloom","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Threshold",1],["Radius",1],["Falloff",1],["Intensity",1],["Steps",1],["HighQuality",1],["Dither",1],["DitherAmount",1],["DitherSeed",1],["Tint",3]],"pos":[420, 40],"text":"    int Count = HighQuality > 0.5 ? 128 : clamp((int)round(Steps), 1, 128);\n    float2 DitherPixel = floor(UV * max(CyCanvasSize.xy, 1.0));\n    float4 Source = Texture2DSample(CyInput, CyInputSampler, UV);\n    float3 BloomSum = 0;\n    float WeightSum = 0;\n    [loop] for (int Index = 0; Index < 128; ++Index) {\n        if (Index >= Count) break;\n        float Jitter = (frac(sin(dot(DitherPixel + float2(Index * 17.17 + DitherSeed, Index * 7.13 - DitherSeed), float2(12.9898, 78.233))) * 43758.5453) - .5) * 2.0 * DitherAmount * Dither;\n        float T = Count <= 1 ? 0.0 : ((Index + Jitter) / (Count - 1.0) - 0.5) * 2.0;\n        float Weight = exp(-abs(T) * Falloff);\n        float2 Offset = float2(T * Radius / max(CyCanvasSize.x, 1.0), 0);\n        BloomSum += max(Texture2DSample(CyInput, CyInputSampler, UV + Offset).rgb - Threshold, 0.0) * Weight;\n        WeightSum += Weight;\n    }\n    return float4(Source.rgb + BloomSum / max(WeightSum, 0.00001) * Intensity * Tint.rgb, Source.a);","value":[0, 0, 0, 0]},{"id":15,"in":[[14, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo full
//@param Threshold float 1 0 16 "Threshold"
//@param Radius float 48 0.25 1024 "Radius"
//@param Falloff float 2 0.01 16 "Falloff"
//@param Intensity float 2 0 64 "Intensity"
//@param Steps float 32 1 128 "Steps"
//@param HighQuality float 0 0 1 "High Quality"
//@param Dither float 1 0 1 "Dither"
//@param DitherAmount float 0.75 0 2 "Dither Amount"
//@param DitherSeed float 0 -1000 1000 "Dither Seed"
//@param Tint color 0.2 0.45 1 1 "Tint"
// Imported preset (Effect / Color & Tone): Quality-controlled horizontal threshold streak for cinematic anamorphic highlights.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Threshold, float Radius, float Falloff, float Intensity, float Steps, float HighQuality, float Dither, float DitherAmount, float DitherSeed, float3 Tint)
{
    int Count = HighQuality > 0.5 ? 128 : clamp((int)round(Steps), 1, 128);
    float2 DitherPixel = floor(UV * max(CyCanvasSize.xy, 1.0));
    float4 Source = Texture2DSample(CyInput, CyInputSampler, UV);
    float3 BloomSum = 0;
    float WeightSum = 0;
    [loop] for (int Index = 0; Index < 128; ++Index) {
        if (Index >= Count) break;
        float Jitter = (frac(sin(dot(DitherPixel + float2(Index * 17.17 + DitherSeed, Index * 7.13 - DitherSeed), float2(12.9898, 78.233))) * 43758.5453) - .5) * 2.0 * DitherAmount * Dither;
        float T = Count <= 1 ? 0.0 : ((Index + Jitter) / (Count - 1.0) - 0.5) * 2.0;
        float Weight = exp(-abs(T) * Falloff);
        float2 Offset = float2(T * Radius / max(CyCanvasSize.x, 1.0), 0);
        BloomSum += max(Texture2DSample(CyInput, CyInputSampler, UV + Offset).rgb - Threshold, 0.0) * Weight;
        WeightSum += Weight;
    }
    return float4(Source.rgb + BloomSum / max(WeightSum, 0.00001) * Intensity * Tint.rgb, Source.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Threshold, Radius, Falloff, Intensity, Steps, HighQuality, Dither, DitherAmount, DitherSeed, Tint.rgb);
}
