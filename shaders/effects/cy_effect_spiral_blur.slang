//@name Spiral Blur
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Spiral Blur","nextId":15,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":512,"min":0,"name":"Radius","op":"param","pos":[40, 330],"text":"Radius","value":[24, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":8,"min":0.0500000007,"name":"Turns","op":"param","pos":[40, 440],"text":"Turns","value":[1, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":6.28299999,"min":-6.28299999,"name":"Rotation","op":"param","pos":[40, 550],"text":"Rotation","value":[0, 0, 0, 0]},{"id":7,"in":[],"index":1,"name":"CenterWeight","op":"param","pos":[40, 660],"text":"Center Weight","value":[0.349999994, 0, 0, 0]},{"id":8,"in":[],"index":1,"max":128,"min":1,"name":"Steps","op":"param","pos":[40, 770],"text":"Steps","value":[24, 0, 0, 0]},{"id":9,"in":[],"index":1,"name":"HighQuality","op":"param","pos":[40, 880],"text":"High Quality","value":[0, 0, 0, 0]},{"id":10,"in":[],"index":1,"name":"Dither","op":"param","pos":[40, 990],"text":"Dither","value":[1, 0, 0, 0]},{"id":11,"in":[],"index":1,"max":2,"min":0,"name":"DitherAmount","op":"param","pos":[40, 1100],"text":"Dither Amount","value":[0.75, 0, 0, 0]},{"id":12,"in":[],"index":1,"max":1000,"min":-1000,"name":"DitherSeed","op":"param","pos":[40, 1210],"text":"Dither Seed","value":[0, 0, 0, 0]},{"id":13,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0],[11, 0],[12, 0]],"name":"Spiral Blur","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Radius",1],["Turns",1],["Rotation",1],["CenterWeight",1],["Steps",1],["HighQuality",1],["Dither",1],["DitherAmount",1],["DitherSeed",1]],"pos":[420, 40],"text":"    int Count = HighQuality > 0.5 ? 128 : clamp((int)round(Steps), 1, 128);\n    float2 DitherPixel = floor(UV * max(CyCanvasSize.xy, 1.0));\n    float4 Sum = 0;\n    float WeightSum = 0;\n    [loop] for (int Index = 0; Index < 128; ++Index) {\n        if (Index >= Count) break;\n        float Jitter = (frac(sin(dot(DitherPixel + float2(Index * 17.17 + DitherSeed, Index * 7.13 - DitherSeed), float2(12.9898, 78.233))) * 43758.5453) - .5) * 2.0 * DitherAmount * Dither;\n        float T = saturate((Index + 0.5 + Jitter) / Count);\n        float Angle = Index * 2.39996323 * Turns + Rotation;\n        float Weight = lerp(1.0, 1.0 - T, saturate(CenterWeight));\n        float2 Offset = float2(cos(Angle), sin(Angle)) * sqrt(T) * Radius / max(CyCanvasSize.xy, 1.0);\n        Sum += Texture2DSample(CyInput, CyInputSampler, UV + Offset) * Weight;\n        WeightSum += Weight;\n    }\n    return Sum / max(WeightSum, 0.00001);","value":[0, 0, 0, 0]},{"id":14,"in":[[13, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo full
//@param Radius float 24 0 512 "Radius"
//@param Turns float 1 0.05 8 "Turns"
//@param Rotation float 0 -6.283 6.283 "Rotation"
//@param CenterWeight float 0.35 0 1 "Center Weight"
//@param Steps float 24 1 128 "Steps"
//@param HighQuality float 0 0 1 "High Quality"
//@param Dither float 1 0 1 "Dither"
//@param DitherAmount float 0.75 0 2 "Dither Amount"
//@param DitherSeed float 0 -1000 1000 "Dither Seed"
// Imported preset (Effect / Blur & Denoise): Golden-angle spiral blur with controllable radius, turns, and sample quality.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float Turns, float Rotation, float CenterWeight, float Steps, float HighQuality, float Dither, float DitherAmount, float DitherSeed)
{
    int Count = HighQuality > 0.5 ? 128 : clamp((int)round(Steps), 1, 128);
    float2 DitherPixel = floor(UV * max(CyCanvasSize.xy, 1.0));
    float4 Sum = 0;
    float WeightSum = 0;
    [loop] for (int Index = 0; Index < 128; ++Index) {
        if (Index >= Count) break;
        float Jitter = (frac(sin(dot(DitherPixel + float2(Index * 17.17 + DitherSeed, Index * 7.13 - DitherSeed), float2(12.9898, 78.233))) * 43758.5453) - .5) * 2.0 * DitherAmount * Dither;
        float T = saturate((Index + 0.5 + Jitter) / Count);
        float Angle = Index * 2.39996323 * Turns + Rotation;
        float Weight = lerp(1.0, 1.0 - T, saturate(CenterWeight));
        float2 Offset = float2(cos(Angle), sin(Angle)) * sqrt(T) * Radius / max(CyCanvasSize.xy, 1.0);
        Sum += Texture2DSample(CyInput, CyInputSampler, UV + Offset) * Weight;
        WeightSum += Weight;
    }
    return Sum / max(WeightSum, 0.00001);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, Turns, Rotation, CenterWeight, Steps, HighQuality, Dither, DitherAmount, DitherSeed);
}
