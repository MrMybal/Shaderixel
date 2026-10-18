//@name Film Grain
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Film Grain","nextId":9,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"name":"Amount","op":"param","pos":[40, 330],"text":"Amount","value":[0.0399999991, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":10000,"min":-10000,"name":"Seed","op":"param","pos":[40, 440],"text":"Seed","value":[0, 0, 0, 0]},{"id":6,"in":[],"index":1,"name":"Colored","op":"param","pos":[40, 550],"text":"Colored","value":[0, 0, 0, 0]},{"id":7,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0]],"name":"Film Grain","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Amount",1],["Seed",1],["Colored",1]],"pos":[420, 40],"text":"    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);\n    float2 p = floor(UV * CyCanvasSize.xy) + Seed;\n    float r = frac(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453) - 0.5;\n    float g = frac(sin(dot(p, float2(39.346, 11.135))) * 24634.6345) - 0.5;\n    float b = frac(sin(dot(p, float2(73.156, 52.235))) * 56445.2345) - 0.5;\n    float3 n = lerp(r.xxx, float3(r, g, b), saturate(Colored));\n    return float4(s.rgb + n * Amount, s.a);","value":[0, 0, 0, 0]},{"id":8,"in":[[7, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo 0
//@param Amount float 0.04 0 1 "Amount"
//@param Seed float 0 -10000 10000 "Seed"
//@param Colored float 0 0 1 "Colored"
// Imported preset (Effect / Stylize): Resolution-aware monochrome or colored film grain.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Amount, float Seed, float Colored)
{
    float4 s = Texture2DSample(CyInput, CyInputSampler, UV);
    float2 p = floor(UV * CyCanvasSize.xy) + Seed;
    float r = frac(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453) - 0.5;
    float g = frac(sin(dot(p, float2(39.346, 11.135))) * 24634.6345) - 0.5;
    float b = frac(sin(dot(p, float2(73.156, 52.235))) * 56445.2345) - 0.5;
    float3 n = lerp(r.xxx, float3(r, g, b), saturate(Colored));
    return float4(s.rgb + n * Amount, s.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Amount, Seed, Colored);
}
