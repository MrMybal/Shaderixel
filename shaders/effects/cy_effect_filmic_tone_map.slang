//@name Filmic Tone Map
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Filmic Tone Map","nextId":15,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":16,"min":-16,"name":"ExposureEV","op":"param","pos":[40, 330],"text":"Exposure EV","value":[0, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":64,"min":0.100000001,"name":"WhitePoint","op":"param","pos":[40, 440],"text":"White Point","value":[11.1999998, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":1,"min":0.00999999978,"name":"A","op":"param","pos":[40, 550],"text":"A","value":[0.150000006, 0, 0, 0]},{"id":7,"in":[],"index":1,"max":2,"min":0.00999999978,"name":"B","op":"param","pos":[40, 660],"text":"B","value":[0.5, 0, 0, 0]},{"id":8,"in":[],"index":1,"name":"C","op":"param","pos":[40, 770],"text":"C","value":[0.100000001, 0, 0, 0]},{"id":9,"in":[],"index":1,"max":2,"min":0.00999999978,"name":"D","op":"param","pos":[40, 880],"text":"D","value":[0.200000003, 0, 0, 0]},{"id":10,"in":[],"index":1,"name":"E","op":"param","pos":[40, 990],"text":"E","value":[0.0199999996, 0, 0, 0]},{"id":11,"in":[],"index":1,"max":2,"min":0.00999999978,"name":"F","op":"param","pos":[40, 1100],"text":"F","value":[0.300000012, 0, 0, 0]},{"id":12,"in":[],"index":1,"name":"Strength","op":"param","pos":[40, 1210],"text":"Strength","value":[1, 0, 0, 0]},{"id":13,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0],[11, 0],[12, 0]],"name":"Filmic Tone Map","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["ExposureEV",1],["WhitePoint",1],["A",1],["B",1],["C",1],["D",1],["E",1],["F",1],["Strength",1]],"pos":[420, 40],"text":"    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);\n    float3 x = max(s.rgb * exp2(ExposureEV), 0.0);\n    float3 v = (x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F) - E / F;\n    float w = max(WhitePoint, 0.001);\n    float white = (w * (A * w + C * B) + D * E) / (w * (A * w + B) + D * F) - E / F;\n    v /= max(white, 0.0001);\n    return float4(lerp(s.rgb, v, saturate(Strength)), s.a);","value":[0, 0, 0, 0]},{"id":14,"in":[[13, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo 0
//@param ExposureEV float 0 -16 16 "Exposure EV"
//@param WhitePoint float 11.2 0.1 64 "White Point"
//@param A float 0.15 0.01 1 "A"
//@param B float 0.5 0.01 2 "B"
//@param C float 0.1 0 1 "C"
//@param D float 0.2 0.01 2 "D"
//@param E float 0.02 0 1 "E"
//@param F float 0.3 0.01 2 "F"
//@param Strength float 1 0 1 "Strength"
// Imported preset (Effect / Color & Tone): Filmic HDR tone mapping with white point, exposure, and blend strength.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float ExposureEV, float WhitePoint, float A, float B, float C, float D, float E, float F, float Strength)
{
    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);
    float3 x = max(s.rgb * exp2(ExposureEV), 0.0);
    float3 v = (x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F) - E / F;
    float w = max(WhitePoint, 0.001);
    float white = (w * (A * w + C * B) + D * E) / (w * (A * w + B) + D * F) - E / F;
    v /= max(white, 0.0001);
    return float4(lerp(s.rgb, v, saturate(Strength)), s.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), ExposureEV, WhitePoint, A, B, C, D, E, F, Strength);
}
