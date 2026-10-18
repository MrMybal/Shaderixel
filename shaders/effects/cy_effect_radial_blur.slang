//@name Radial Blur
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Radial Blur","nextId":13,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":1,"min":-1,"name":"Amount","op":"param","pos":[40, 330],"text":"Amount","value":[0.119999997, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":128,"min":1,"name":"Steps","op":"param","pos":[40, 440],"text":"Steps","value":[16, 0, 0, 0]},{"id":6,"in":[],"index":1,"name":"HighQuality","op":"param","pos":[40, 550],"text":"High Quality","value":[0, 0, 0, 0]},{"id":7,"in":[],"index":1,"name":"Dither","op":"param","pos":[40, 660],"text":"Dither","value":[1, 0, 0, 0]},{"id":8,"in":[],"index":1,"max":2,"min":0,"name":"DitherAmount","op":"param","pos":[40, 770],"text":"Dither Amount","value":[0.75, 0, 0, 0]},{"id":9,"in":[],"index":1,"max":1000,"min":-1000,"name":"DitherSeed","op":"param","pos":[40, 880],"text":"Dither Seed","value":[0, 0, 0, 0]},{"id":10,"in":[],"index":1,"name":"Center","op":"param2","pos":[40, 990],"text":"Center","value":[0.5, 0.5, 0, 0]},{"id":11,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0]],"name":"Radial Blur","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Amount",1],["Steps",1],["HighQuality",1],["Dither",1],["DitherAmount",1],["DitherSeed",1],["Center",3]],"pos":[420, 40],"text":"    int Count = HighQuality > 0.5 ? 128 : clamp((int)round(Steps), 1, 128);\n    float2 DitherPixel = floor(UV * max(CyCanvasSize.xy, 1.0));\n    float2 Direction = (UV - Center.xy) * Amount;\n    float4 Sum = 0;\n    [loop] for (int Index = 0; Index < 128; ++Index) {\n        if (Index >= Count) break;\n        float Jitter = (frac(sin(dot(DitherPixel + float2(Index * 17.17 + DitherSeed, Index * 7.13 - DitherSeed), float2(12.9898, 78.233))) * 43758.5453) - .5) * 2.0 * DitherAmount * Dither;\n        float T = Count <= 1 ? 0.0 : Index / (Count - 1.0);\n        T = saturate(T + Jitter / max((float)Count, 1.0));\n        Sum += Texture2DSample(CyInput, CyInputSampler, UV - Direction * T);\n    }\n    return Sum / max((float)Count, 1.0);","value":[0, 0, 0, 0]},{"id":12,"in":[[11, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo full
//@param Amount float 0.12 -1 1 "Amount"
//@param Steps float 16 1 128 "Steps"
//@param HighQuality float 0 0 1 "High Quality"
//@param Dither float 1 0 1 "Dither"
//@param DitherAmount float 0.75 0 2 "Dither Amount"
//@param DitherSeed float 0 -1000 1000 "Dither Seed"
//@param Center float2 0.5 0.5 0 1 "Center"
// Imported preset (Effect / Blur & Denoise): Quality-controlled radial zoom blur centered at an arbitrary UV.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Amount, float Steps, float HighQuality, float Dither, float DitherAmount, float DitherSeed, float3 Center)
{
    int Count = HighQuality > 0.5 ? 128 : clamp((int)round(Steps), 1, 128);
    float2 DitherPixel = floor(UV * max(CyCanvasSize.xy, 1.0));
    float2 Direction = (UV - Center.xy) * Amount;
    float4 Sum = 0;
    [loop] for (int Index = 0; Index < 128; ++Index) {
        if (Index >= Count) break;
        float Jitter = (frac(sin(dot(DitherPixel + float2(Index * 17.17 + DitherSeed, Index * 7.13 - DitherSeed), float2(12.9898, 78.233))) * 43758.5453) - .5) * 2.0 * DitherAmount * Dither;
        float T = Count <= 1 ? 0.0 : Index / (Count - 1.0);
        T = saturate(T + Jitter / max((float)Count, 1.0));
        Sum += Texture2DSample(CyInput, CyInputSampler, UV - Direction * T);
    }
    return Sum / max((float)Count, 1.0);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Amount, Steps, HighQuality, Dither, DitherAmount, DitherSeed, float3(Center, 0.0));
}
