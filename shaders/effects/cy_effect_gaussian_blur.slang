//@name Gaussian Blur
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Gaussian Blur","nextId":14,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":512,"min":0,"name":"Radius","op":"param","pos":[40, 330],"text":"Radius","value":[16, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":256,"min":0.00999999978,"name":"Sigma","op":"param","pos":[40, 440],"text":"Sigma","value":[6, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":6.28299999,"min":-6.28299999,"name":"Angle","op":"param","pos":[40, 550],"text":"Angle","value":[0, 0, 0, 0]},{"id":7,"in":[],"index":1,"max":128,"min":1,"name":"Steps","op":"param","pos":[40, 660],"text":"Steps","value":[24, 0, 0, 0]},{"id":8,"in":[],"index":1,"name":"HighQuality","op":"param","pos":[40, 770],"text":"High Quality","value":[0, 0, 0, 0]},{"id":9,"in":[],"index":1,"name":"Dither","op":"param","pos":[40, 880],"text":"Dither","value":[1, 0, 0, 0]},{"id":10,"in":[],"index":1,"max":2,"min":0,"name":"DitherAmount","op":"param","pos":[40, 990],"text":"Dither Amount","value":[0.75, 0, 0, 0]},{"id":11,"in":[],"index":1,"max":1000,"min":-1000,"name":"DitherSeed","op":"param","pos":[40, 1100],"text":"Dither Seed","value":[0, 0, 0, 0]},{"id":12,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0],[11, 0]],"name":"Gaussian Blur","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Radius",1],["Sigma",1],["Angle",1],["Steps",1],["HighQuality",1],["Dither",1],["DitherAmount",1],["DitherSeed",1]],"pos":[420, 40],"text":"    int Count = HighQuality > 0.5 ? 128 : clamp((int)round(Steps), 1, 128);\n    float2 DitherPixel = floor(UV * max(CyCanvasSize.xy, 1.0));\n    float2 Direction = float2(cos(Angle) / max(CyCanvasSize.x, 1.0), sin(Angle) / max(CyCanvasSize.y, 1.0));\n    float4 Sum = 0;\n    float WeightSum = 0;\n    [loop] for (int Index = 0; Index < 128; ++Index) {\n        if (Index >= Count) break;\n        float Jitter = (frac(sin(dot(DitherPixel + float2(Index * 17.17 + DitherSeed, Index * 7.13 - DitherSeed), float2(12.9898, 78.233))) * 43758.5453) - .5) * 2.0 * DitherAmount * Dither;\n        float T = Count <= 1 ? 0.0 : (Index / (Count - 1.0) - 0.5) * 2.0;\n        T += Jitter * 2.0 / max((float)Count, 1.0);\n        float Pixel = T * Radius;\n        float Weight = exp(-0.5 * Pixel * Pixel / max(Sigma * Sigma, 0.0001));\n        Sum += Texture2DSample(CyInput, CyInputSampler, UV + Direction * Pixel) * Weight;\n        WeightSum += Weight;\n    }\n    return Sum / max(WeightSum, 0.00001);","value":[0, 0, 0, 0]},{"id":13,"in":[[12, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo full
//@param Radius float 16 0 512 "Radius"
//@param Sigma float 6 0.01 256 "Sigma"
//@param Angle float 0 -6.283 6.283 "Angle"
//@param Steps float 24 1 128 "Steps"
//@param HighQuality float 0 0 1 "High Quality"
//@param Dither float 1 0 1 "Dither"
//@param DitherAmount float 0.75 0 2 "Dither Amount"
//@param DitherSeed float 0 -1000 1000 "Dither Seed"
// Imported preset (Effect / Blur & Denoise): High-quality directional Gaussian blur; stack horizontal and vertical instances for a full 2D Gaussian.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float Sigma, float Angle, float Steps, float HighQuality, float Dither, float DitherAmount, float DitherSeed)
{
    int Count = HighQuality > 0.5 ? 128 : clamp((int)round(Steps), 1, 128);
    float2 DitherPixel = floor(UV * max(CyCanvasSize.xy, 1.0));
    float2 Direction = float2(cos(Angle) / max(CyCanvasSize.x, 1.0), sin(Angle) / max(CyCanvasSize.y, 1.0));
    float4 Sum = 0;
    float WeightSum = 0;
    [loop] for (int Index = 0; Index < 128; ++Index) {
        if (Index >= Count) break;
        float Jitter = (frac(sin(dot(DitherPixel + float2(Index * 17.17 + DitherSeed, Index * 7.13 - DitherSeed), float2(12.9898, 78.233))) * 43758.5453) - .5) * 2.0 * DitherAmount * Dither;
        float T = Count <= 1 ? 0.0 : (Index / (Count - 1.0) - 0.5) * 2.0;
        T += Jitter * 2.0 / max((float)Count, 1.0);
        float Pixel = T * Radius;
        float Weight = exp(-0.5 * Pixel * Pixel / max(Sigma * Sigma, 0.0001));
        Sum += Texture2DSample(CyInput, CyInputSampler, UV + Direction * Pixel) * Weight;
        WeightSum += Weight;
    }
    return Sum / max(WeightSum, 0.00001);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, Sigma, Angle, Steps, HighQuality, Dither, DitherAmount, DitherSeed);
}
