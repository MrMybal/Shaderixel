//@name Pencil Sketch
//@category Traitements
//@graph {"category":"Traitements","inputs":1,"name":"Pencil Sketch","nextId":16,"nodes":[{"id":1,"in":[],"name":"CyInput","op":"texture","pos":[40, 40],"value":[0, 0, 0, 0]},{"id":2,"in":[],"op":"uv","pos":[40, 150],"value":[0, 0, 0, 0]},{"id":3,"in":[],"op":"canvassize","pos":[40, 240],"value":[0, 0, 0, 0]},{"id":4,"in":[],"index":1,"max":16,"min":0.25,"name":"Radius","op":"param","pos":[40, 330],"text":"Radius","value":[1.5, 0, 0, 0]},{"id":5,"in":[],"index":1,"max":64,"min":0,"name":"EdgeStrength","op":"param","pos":[40, 440],"text":"Edge Strength","value":[5, 0, 0, 0]},{"id":6,"in":[],"index":1,"max":8,"min":0.0500000007,"name":"EdgePower","op":"param","pos":[40, 550],"text":"Edge Power","value":[0.699999988, 0, 0, 0]},{"id":7,"in":[],"index":1,"max":2,"min":0,"name":"Shading","op":"param","pos":[40, 660],"text":"Shading","value":[0.449999988, 0, 0, 0]},{"id":8,"in":[],"index":1,"max":4096,"min":2,"name":"GrainScale","op":"param","pos":[40, 770],"text":"Grain Scale","value":[512, 0, 0, 0]},{"id":9,"in":[],"index":1,"name":"GrainAmount","op":"param","pos":[40, 880],"text":"Grain Amount","value":[0.0799999982, 0, 0, 0]},{"id":10,"in":[],"index":1,"name":"PreserveColor","op":"param","pos":[40, 990],"text":"Preserve Color","value":[0, 0, 0, 0]},{"id":11,"in":[],"index":1,"name":"Strength","op":"param","pos":[40, 1100],"text":"Strength","value":[1, 0, 0, 0]},{"id":12,"in":[],"index":1,"name":"PencilColor","op":"paramcolor","pos":[40, 1210],"text":"Pencil Color","value":[0.0149999997, 0.0120000001, 0.00999999978, 1]},{"id":13,"in":[],"index":1,"name":"PaperColor","op":"paramcolor","pos":[40, 1320],"text":"Paper Color","value":[0.899999976, 0.860000014, 0.74000001, 1]},{"id":14,"in":[[1, 0],[2, 0],[3, 0],[4, 0],[5, 0],[6, 0],[7, 0],[8, 0],[9, 0],[10, 0],[11, 0],[12, 0],[13, 0]],"name":"Pencil Sketch","op":"custom","outs":[["Resultat",4]],"pins":[["CyInput",5],["UV",2],["CyCanvasSize",4],["Radius",1],["EdgeStrength",1],["EdgePower",1],["Shading",1],["GrainScale",1],["GrainAmount",1],["PreserveColor",1],["Strength",1],["PencilColor",3],["PaperColor",3]],"pos":[420, 40],"text":"    float2 t = Radius / max(CyCanvasSize.xy, 1.0);\n    float4 c = Texture2DSample(CyInput, CyInputSampler, UV);\n    float l = dot(c.rgb, float3(.2126, .7152, .0722));\n    float lx = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(t.x, 0)).rgb, float3(.2126, .7152, .0722)) - dot(Texture2DSample(CyInput, CyInputSampler, UV - float2(t.x, 0)).rgb, float3(.2126, .7152, .0722));\n    float ly = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(0, t.y)).rgb, float3(.2126, .7152, .0722)) - dot(Texture2DSample(CyInput, CyInputSampler, UV - float2(0, t.y)).rgb, float3(.2126, .7152, .0722));\n    float edge = pow(saturate(length(float2(lx, ly)) * EdgeStrength), EdgePower);\n    float grain = frac(sin(dot(floor(UV * GrainScale), float2(12.9898, 78.233))) * 43758.5453);\n    float shade = saturate(1.0 - edge - (1.0 - l) * Shading - (grain - .5) * GrainAmount);\n    float3 mono = lerp(PencilColor.rgb, PaperColor.rgb, shade);\n    float3 outColor = lerp(mono, mono * c.rgb * 1.5, PreserveColor);\n    return float4(lerp(c.rgb, outColor, Strength), c.a);","value":[0, 0, 0, 0]},{"id":15,"in":[[14, 0],[0, 0],[0, 0]],"op":"output","pos":[780, 40],"value":[0, 0, 0, 0]}],"premultiplied":false}
//@input Image
//@halo full
//@param Radius float 1.5 0.25 16 "Radius"
//@param EdgeStrength float 5 0 64 "Edge Strength"
//@param EdgePower float 0.7 0.05 8 "Edge Power"
//@param Shading float 0.45 0 2 "Shading"
//@param GrainScale float 512 2 4096 "Grain Scale"
//@param GrainAmount float 0.08 0 1 "Grain Amount"
//@param PreserveColor float 0 0 1 "Preserve Color"
//@param Strength float 1 0 1 "Strength"
//@param PencilColor color 0.015 0.012 0.01 1 "Pencil Color"
//@param PaperColor color 0.9 0.86 0.74 1 "Paper Color"
// Imported preset (Effect / Stylize): Pencil-sketch conversion with multi-scale edges, paper tone, grain, and source-color retention.
// Unreal Custom-node code kept as written; Texture2DSample reads the effect inputs.
float4 CyTex(uint i, float2 uv) {
    return HasInput(i) ? InputUV(i, uv) : float4(1.0, 1.0, 1.0, 1.0);
}
#define Texture2DSample(T, S, UVS) CyTex(T, UVS)
static const uint CyInput = 0;
float4 CyPreset(float2 UV, float4 CyCanvasSize, float Radius, float EdgeStrength, float EdgePower, float Shading, float GrainScale, float GrainAmount, float PreserveColor, float Strength, float3 PencilColor, float3 PaperColor)
{
    float2 t = Radius / max(CyCanvasSize.xy, 1.0);
    float4 c = Texture2DSample(CyInput, CyInputSampler, UV);
    float l = dot(c.rgb, float3(.2126, .7152, .0722));
    float lx = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(t.x, 0)).rgb, float3(.2126, .7152, .0722)) - dot(Texture2DSample(CyInput, CyInputSampler, UV - float2(t.x, 0)).rgb, float3(.2126, .7152, .0722));
    float ly = dot(Texture2DSample(CyInput, CyInputSampler, UV + float2(0, t.y)).rgb, float3(.2126, .7152, .0722)) - dot(Texture2DSample(CyInput, CyInputSampler, UV - float2(0, t.y)).rgb, float3(.2126, .7152, .0722));
    float edge = pow(saturate(length(float2(lx, ly)) * EdgeStrength), EdgePower);
    float grain = frac(sin(dot(floor(UV * GrainScale), float2(12.9898, 78.233))) * 43758.5453);
    float shade = saturate(1.0 - edge - (1.0 - l) * Shading - (grain - .5) * GrainAmount);
    float3 mono = lerp(PencilColor.rgb, PaperColor.rgb, shade);
    float3 outColor = lerp(mono, mono * c.rgb * 1.5, PreserveColor);
    return float4(lerp(c.rgb, outColor, Strength), c.a);
}
float4 Effect(EffectContext ctx)
{
    return CyPreset(ctx.uv, float4(ctx.size, ctx.texel), Radius, EdgeStrength, EdgePower, Shading, GrainScale, GrainAmount, PreserveColor, Strength, PencilColor.rgb, PaperColor.rgb);
}
