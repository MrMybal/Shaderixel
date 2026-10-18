//@name Impulse Denoise
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Impulse Denoise","nextId":11,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":32,"min":0.25,"name":"Radius","op":"param","pos":[40, 330],"text":"Radius","value":[2, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":4,"min":0.00100000005,"name":"ColorTolerance","op":"param","pos":[40, 440],"text":"Color Tolerance","value":[0.119999997, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":2,"min":0,"name":"Threshold","op":"param","pos":[40, 550],"text":"Threshold","value":[0.0799999982, 0, 0, 0]},{"id":7,"in":[],"index":1,"max":1,"min":9.99999975e-05,"name":"Softness","op":"param","pos":[40, 660],"text":"Softness","value":[0.0500000007, 0, 0, 0]},{"id":8,"in":[],"index":1,"name":"Strength","op":"param","pos":[40, 770],"text":"Strength","value":[1, 0, 0, 0]},{"id":9,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0]],"name":"Impulse Denoise","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Radius",1],["ColorTolerance",1],["Threshold",1],["Softness",1],["Strength",1]],"pos":[420, 40],"text":"    float2 t = Radius / max(CyCanvasSize.xy, 1.0);\n    float4 c = Texture2DSample(CyInput, CyInputSampler, UV);\n    float3 sum = 0;\n    float wsum = 0;\n    [unroll] for (int i = 0; i < 16; i++) {\n        float a = i * 2.39996323;\n        float2 o = float2(cos(a), sin(a)) * t * sqrt((i + .5) / 16.0);\n        float3 s = Texture2DSample(CyInput, CyInputSampler, UV + o).rgb;\n        float delta = length(s - c.rgb);\n        float w = exp(-delta * delta / max(ColorTolerance * ColorTolerance, .000001));\n        sum += s * w;\n        wsum += w;\n    }\n    float3 filtered = sum / max(wsum, .0001);\n    float centerError = length(c.rgb - filtered);\n    float reject = smoothstep(Threshold, Threshold + Softness, centerError);\n    return float4(lerp(c.rgb, filtered, reject * saturate(Strength)), c.a);","value":[0, 0, 0, 0]},{"id":10,"in":[[9, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo full
//@param Radius float 2 0.25 32 "Radius"
//@param ColorTolerance float 0.12 0.001 4 "Color Tolerance"
//@param Threshold float 0.08 0 2 "Threshold"
//@param Softness float 0.05 0.0001 1 "Softness"
//@param Strength float 1 0 1 "Strength"
// Imported preset (Effect / Blur & Denoise): Edge-preserving impulse and salt-and-pepper denoiser with adaptive color rejection.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float ColorTolerance, float Threshold, float Softness, float Strength)
{
    float2 t = Radius / max(CyCanvasSize.xy, 1.0);
    float4 c = Texture2DSample(CyInput, CyInputSampler, UV);
    float3 sum = 0;
    float wsum = 0;
    [unroll] for (int i = 0; i < 16; i++) {
        float a = i * 2.39996323;
        float2 o = float2(cos(a), sin(a)) * t * sqrt((i + .5) / 16.0);
        float3 s = Texture2DSample(CyInput, CyInputSampler, UV + o).rgb;
        float delta = length(s - c.rgb);
        float w = exp(-delta * delta / max(ColorTolerance * ColorTolerance, .000001));
        sum += s * w;
        wsum += w;
    }
    float3 filtered = sum / max(wsum, .0001);
    float centerError = length(c.rgb - filtered);
    float reject = smoothstep(Threshold, Threshold + Softness, centerError);
    return float4(lerp(c.rgb, filtered, reject * saturate(Strength)), c.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, ColorTolerance, Threshold, Softness, Strength);
}
